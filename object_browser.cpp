#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/regex.h"
#include <algorithm>
#include <set>

#include "object_browser.h"
#include "object_browser_sql.h"
#include "pqwx_frame.h"

#define SQL(t) _SQL__##t

const int EVENT_WORK_FINISHED = 10000;

BEGIN_EVENT_TABLE(ObjectBrowser, wxTreeCtrl)
  EVT_TREE_ITEM_EXPANDING(Pqwx_ObjectBrowser, ObjectBrowser::BeforeExpand)
  EVT_TREE_ITEM_GETTOOLTIP(Pqwx_ObjectBrowser, ObjectBrowser::OnGetTooltip)
  EVT_COMMAND(EVENT_WORK_FINISHED, wxEVT_COMMAND_TEXT_UPDATED, ObjectBrowser::OnWorkFinished)
END_EVENT_TABLE()

typedef std::vector< std::vector<wxString> > QueryResults;

static wxRegEx serverVersionRegex(wxT("^([^ ]+ ([0-9]+)\\.([0-9]+)([0-9.a-z]*))"));

class ObjectModel : public wxTreeItemData {
public:
  wxString name;
  wxString description;
};

class ColumnModel : public ObjectModel {
public:
  RelationModel *relation;
  wxString type;
  bool nullable;
  bool hasDefault;
};

class IndexModel : public ObjectModel {
public:
  RelationModel *relation;
  vector<wxString> columns;
  wxString type;
};

class TriggerModel : public ObjectModel {
public:
  RelationModel *relation;
};

class SchemaMemberModel : public ObjectModel {
public:
  DatabaseModel *database;
  unsigned long oid;
  wxString schema;
  bool user;
};

class RelationModel : public SchemaMemberModel {
public:
  enum Type { TABLE, VIEW, SEQUENCE } type;
};

class FunctionModel : public SchemaMemberModel {
public:
  wxString prototype;
  enum Type { SCALAR, RECORDSET, TRIGGER, AGGREGATE, WINDOW } type;
};

class DatabaseModel : public ObjectModel {
public:
  unsigned long oid;
  bool isTemplate;
  bool allowConnections;
  bool havePrivsToConnect;
  ServerModel *server;
  bool IsUsable() {
    return allowConnections && havePrivsToConnect;
  }
  bool IsSystem() {
    return name.IsSameAs(_T("postgres")) || name.IsSameAs(_T("template0")) || name.IsSameAs(_T("template1"));
  }
};

class RoleModel : public ObjectModel {
public:
  unsigned long oid;
  bool superuser;
  bool canLogin;
};

class ServerModel : public wxTreeItemData {
public:
  ServerConnection *conn;
  vector<DatabaseModel*> databases;
  int majorVersion;
  int minorVersion;
  wxString versionSuffix;
  void SetVersion(int major, int minor, wxString suffix) {
    majorVersion = major;
    minorVersion = minor;
    versionSuffix = suffix;
  }
  DatabaseModel *findDatabase(unsigned long oid) {
    for (vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
      if ((*iter)->oid == oid)
	return *iter;
    }
    return NULL;
  }
  bool versionNotBefore(int major, int minor) {
    return majorVersion > major || majorVersion == major && minorVersion >= minor;
  }
};

class ObjectBrowserWork : public DatabaseWork {
public:
  ObjectBrowserWork(wxEvtHandler *owner) : dest(owner) {}
  virtual ~ObjectBrowserWork() {}
  void execute(SqlLogger *logger_, PGconn *conn) {
    logger = logger_;
    if (!cmd(conn, "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY"))
      return;

    executeInTransaction(conn);

    cmd(conn, "END");
  }
  virtual void loadResultsToGui(ObjectBrowser *browser) = 0;
protected:
  SqlLogger *logger;
  void notifyFinished() {
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_WORK_FINISHED);
    event.SetClientData(this);
    dest->AddPendingEvent(event);
  }
  virtual void executeInTransaction(PGconn *conn) = 0;
  bool cmd(PGconn *conn, const char *sql) {
    DatabaseCommandWork work(sql);
    work.execute(logger, conn);
    return work.successful;
  }
  bool doQuery(PGconn *conn, const char *sql, QueryResults &results, Oid paramType, const char *paramValue) {
    logger->LogSql(sql);

    PGresult *rs = PQexecParams(conn, sql, 1, &paramType, &paramValue, NULL, NULL, 0);
    if (!rs)
      return false;

    ExecStatusType status = PQresultStatus(rs);
    if (status != PGRES_TUPLES_OK) {
#ifdef PQWX_DEBUG
      logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
      return false; // expected data back
    }

    loadResultSet(rs, results);

    PQclear(rs);

    return true;
  }
  bool doQuery(PGconn *conn, const char *sql, QueryResults &results) {
    logger->LogSql(sql);

    PGresult *rs = PQexec(conn, sql);
    if (!rs)
      return false;

    ExecStatusType status = PQresultStatus(rs);
    if (status != PGRES_TUPLES_OK) {
#ifdef PQWX_DEBUG
      logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
      return false; // expected data back
    }

    loadResultSet(rs, results);

    PQclear(rs);

    return true;
  }
  void loadResultSet(PGresult *rs, QueryResults &results) {
    int rowCount = PQntuples(rs);
    int colCount = PQnfields(rs);
    for (int rowNum = 0; rowNum < rowCount; rowNum++) {
      vector<wxString> row;
      for (int colNum = 0; colNum < colCount; colNum++) {
	const char *value = PQgetvalue(rs, rowNum, colNum);
	row.push_back(wxString(value, wxConvUTF8));
      }
      results.push_back(row);
    }
  }
private:
  wxEvtHandler *dest;
};

#define CHECK_INDEX(iter, index) wxASSERT(index < (*iter).size())
#define GET_OID(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToULong(&(result))
#define GET_BOOLEAN(iter, index, result) CHECK_INDEX(iter, index); result = (*iter)[index].IsSameAs(_T("t"))
#define GET_TEXT(iter, index, result) CHECK_INDEX(iter, index); result = (*iter)[index]
#define GET_INT4(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToULong(&(result))

class RefreshDatabaseListWork : public ObjectBrowserWork {
public:
  RefreshDatabaseListWork(wxEvtHandler *owner, ServerModel *serverModel, wxTreeItemId serverItem) : ObjectBrowserWork(owner), serverModel(serverModel), serverItem(serverItem) {
    wxLogDebug(_T("%p: work to load database list"), this);
  }
protected:
  void executeInTransaction(PGconn *conn) {
    loadServer(conn);
    loadDatabases(conn);
    loadRoles(conn);
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    ob->FillInServer(serverModel, serverItem, serverVersion);
    ob->FillInDatabases(serverModel, serverItem, databases);
    ob->FillInRoles(serverModel, serverItem, roles);

    ob->Expand(serverItem);
  }
private:
  ServerModel *serverModel;
  wxTreeItemId serverItem;
  vector<DatabaseModel*> databases;
  vector<RoleModel*> roles;
  wxString serverVersion;
  void loadServer(PGconn *conn) {
    QueryResults serverRows;
    doQuery(conn, SQL(Server), serverRows);
    serverVersion = serverRows[0][0];
  }
  void loadDatabases(PGconn *conn) {
    QueryResults databaseRows;
    doQuery(conn, SQL(Databases), databaseRows);
    for (QueryResults::iterator iter = databaseRows.begin(); iter != databaseRows.end(); iter++) {
      DatabaseModel *database = new DatabaseModel();
      database->server = serverModel;
      GET_OID(iter, 0, database->oid);
      GET_TEXT(iter, 1, database->name);
      GET_BOOLEAN(iter, 2, database->isTemplate);
      GET_BOOLEAN(iter, 3, database->allowConnections);
      GET_BOOLEAN(iter, 4, database->havePrivsToConnect);
      GET_TEXT(iter, 5, database->description);
      databases.push_back(database);
    }
  }
  void loadRoles(PGconn *conn) {
    QueryResults roleRows;
    doQuery(conn, SQL(Roles), roleRows);
    for (QueryResults::iterator iter = roleRows.begin(); iter != roleRows.end(); iter++) {
      RoleModel *role = new RoleModel();
      GET_OID(iter, 0, role->oid);
      GET_TEXT(iter, 1, role->name);
      GET_BOOLEAN(iter, 2, role->canLogin);
      GET_BOOLEAN(iter, 3, role->superuser);
      GET_TEXT(iter, 4, role->description);
      roles.push_back(role);
    }
  }
};

static inline bool systemSchema(wxString schema) {
  return schema.StartsWith(_T("pg_")) || schema.IsSameAs(_T("information_schema"));
}

static inline bool emptySchema(vector<RelationModel*> schemaRelations) {
  return schemaRelations.size() == 1 && schemaRelations[0]->name.IsSameAs(_T(""));
}

class LoadDatabaseSchemaWork : public ObjectBrowserWork {
public:
  LoadDatabaseSchemaWork(wxEvtHandler *owner, DatabaseModel *databaseModel, wxTreeItemId databaseItem) : ObjectBrowserWork(owner), databaseModel(databaseModel), databaseItem(databaseItem) {
    wxLogDebug(_T("%p: work to load schema"), this);
  }
private:
  DatabaseModel *databaseModel;
  wxTreeItemId databaseItem;
  vector<RelationModel*> relations;
  vector<FunctionModel*> functions;
protected:
  void executeInTransaction(PGconn *conn) {
    loadRelations(conn);
    loadFunctions(conn);
  }
  void loadRelations(PGconn *conn) {
    QueryResults relationRows;
    map<wxString, RelationModel::Type> typemap;
    typemap[_T("r")] = RelationModel::TABLE;
    typemap[_T("v")] = RelationModel::VIEW;
    typemap[_T("S")] = RelationModel::SEQUENCE;
    doQuery(conn, SQL(Relations), relationRows);
    for (QueryResults::iterator iter = relationRows.begin(); iter != relationRows.end(); iter++) {
      RelationModel *relation = new RelationModel();
      relation->database = databaseModel;
      GET_OID(iter, 0, relation->oid);
      GET_TEXT(iter, 1, relation->schema);
      GET_TEXT(iter, 2, relation->name);
      wxString relkind;
      GET_TEXT(iter, 3, relkind);
      relation->type = typemap[relkind];
      relation->user = !systemSchema(relation->schema);
      GET_TEXT(iter, 4, relation->description);
      relations.push_back(relation);
    }
  }
  void loadFunctions(PGconn *conn) {
    QueryResults functionRows;
    map<wxString, FunctionModel::Type> typemap;
    typemap[_T("f")] = FunctionModel::SCALAR;
    typemap[_T("fs")] = FunctionModel::RECORDSET;
    typemap[_T("fa")] = FunctionModel::AGGREGATE;
    typemap[_T("fw")] = FunctionModel::WINDOW;
    if (databaseModel->server->versionNotBefore(8,4))
      doQuery(conn, SQL(Functions), functionRows);
    else
      doQuery(conn, SQL(Functions83), functionRows);
    for (QueryResults::iterator iter = functionRows.begin(); iter != functionRows.end(); iter++) {
      FunctionModel *func = new FunctionModel();
      func->database = databaseModel;
      GET_OID(iter, 0, func->oid);
      GET_TEXT(iter, 1, func->schema);
      GET_TEXT(iter, 2, func->name);
      GET_TEXT(iter, 3, func->prototype);
      wxString type;
      GET_TEXT(iter, 4, type);
      func->type = typemap[type];
      GET_TEXT(iter, 5, func->description);
      func->user = !systemSchema(func->schema);
      functions.push_back(func);
    }
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    ob->FillInDatabaseSchema(databaseModel, databaseItem, relations, functions);
    ob->Expand(databaseItem);
    ob->SetItemText(databaseItem, databaseModel->name); // remove loading message
  }
};

class LoadRelationWork : public ObjectBrowserWork {
public:
  LoadRelationWork(wxEvtHandler *owner, RelationModel *relationModel, wxTreeItemId relationItem) : ObjectBrowserWork(owner), relationModel(relationModel), relationItem(relationItem) {
    wxLogDebug(_T("%p: work to load relation"), this);
  }
private:
  RelationModel *relationModel;
  wxTreeItemId relationItem;
  vector<ColumnModel*> columns;
  vector<IndexModel*> indices;
  vector<TriggerModel*> triggers;
protected:
  void executeInTransaction(PGconn *conn) {
    loadColumns(conn);
    loadIndices(conn);
    loadTriggers(conn);
  }
private:
  void loadColumns(PGconn *conn) {
    QueryResults attributeRows;
    wxString oidValue;
    oidValue.Printf(_T("%d"), relationModel->oid);
    doQuery(conn, SQL(Columns), attributeRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = attributeRows.begin(); iter != attributeRows.end(); iter++) {
      ColumnModel *column = new ColumnModel();
      column->relation = relationModel;
      GET_TEXT(iter, 0, column->name);
      GET_TEXT(iter, 1, column->type);
      GET_BOOLEAN(iter, 2, column->nullable);
      GET_BOOLEAN(iter, 3, column->hasDefault);
      GET_TEXT(iter, 4, column->description);
      columns.push_back(column);
    }
  }
  void loadIndices(PGconn *conn) {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults indexRows;
    doQuery(conn, "SELECT relname, indisunique, indisprimary, indisexclusion, indisclustered FROM pg_index JOIN pg_class ON pg_class.oid = pg_index.indexrelid WHERE indrelid = $1", indexRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = indexRows.begin(); iter != indexRows.end(); iter++) {
      IndexModel *index = new IndexModel();
      GET_TEXT(iter, 0, index->name);
      indices.push_back(index);
    }
  }
  void loadTriggers(PGconn *conn) {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults triggerRows;
    doQuery(conn, "SELECT tgname, tgfoid::regprocedure, tgenabled FROM pg_trigger WHERE tgrelid = $1 AND NOT tgisinternal", triggerRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = triggerRows.begin(); iter != triggerRows.end(); iter++) {
      TriggerModel *trigger = new TriggerModel();
      GET_TEXT(iter, 0, trigger->name);
      triggers.push_back(trigger);
    }
  }
protected:
  void loadResultsToGui(ObjectBrowser *ob) {
    ob->FillInRelation(relationModel, relationItem, columns, indices, triggers);
    ob->Expand(relationItem);

    // remove 'loading...' tag
    wxString itemText = ob->GetItemText(relationItem);
    int space = itemText.Find(_T(' '));
    if (space != wxNOT_FOUND) {
      ob->SetItemText(relationItem, itemText.Left(space));
    }
  }
};

class LazyLoader : public wxTreeItemData {
public:
  virtual void load(wxTreeItemId parent) = 0;
};

class DatabaseLoader : public LazyLoader {
public:
  DatabaseLoader(ObjectBrowser *ob, DatabaseModel *db) : ob(ob), db(db) {}

  void load(wxTreeItemId parent) {
    ob->LoadDatabase(parent, db);
  }
  
private:
  DatabaseModel *db;
  ObjectBrowser *ob;
};

class RelationLoader : public LazyLoader {
public:
  RelationLoader(ObjectBrowser *ob, RelationModel *rel) : ob(ob), rel(rel) {}

  void load(wxTreeItemId parent) {
    ob->LoadRelation(parent, rel);
  }
  
private:
  RelationModel *rel;
  ObjectBrowser *ob;
};

ObjectBrowser::ObjectBrowser(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style) : wxTreeCtrl(parent, id, pos, size, style) {
  AddRoot(_T("root"));
}

void ObjectBrowser::AddServerConnection(ServerConnection *conn) {
  ServerModel *serverModel = new ServerModel();
  serverModel->conn = conn;
  servers.push_back(serverModel);

  // setting the text twice is a bug workaround for wx 2.8
  // see http://trac.wxwidgets.org/ticket/10085
  wxLogDebug(_T("Connection added to object browser: %s"), conn->Identification().c_str());
  wxTreeItemId serverItem = AppendItem(GetRootItem(), conn->Identification());
  SetItemText(serverItem, conn->Identification());
  SetItemData(serverItem, serverModel);

  RefreshDatabaseList(serverItem);
}

void ObjectBrowser::dispose() {
  for (std::vector<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    (*iter)->conn->CloseAllSync();
  }
}

void ObjectBrowser::RefreshDatabaseList(wxTreeItemId serverItem) {
  ServerModel *serverModel = dynamic_cast<ServerModel*>(GetItemData(serverItem));
  SubmitServerWork(serverModel, new RefreshDatabaseListWork(this, serverModel, serverItem));
}

void ObjectBrowser::SubmitServerWork(ServerModel *serverModel, DatabaseWork *work) {
  DatabaseConnection *conn = serverModel->conn->getConnection();
  // bodge
  if (!conn->IsConnected())
    conn->Connect();
  conn->AddWork(work);
}

void ObjectBrowser::OnWorkFinished(wxCommandEvent &e) {
  ObjectBrowserWork *work = static_cast<ObjectBrowserWork*>(e.GetClientData());

  if (work->isDone()) {
    wxLogDebug(_T("%p: work finished"), work);
    work->loadResultsToGui(this);
  }
  else {
    wxLogDebug(_T("%p: work finished but not marked as done"), work);
    // call a cleanup method?
  }

  delete work;
}

void ObjectBrowser::BeforeExpand(wxTreeEvent &event) {
  wxTreeItemIdValue cookie;
  wxTreeItemId expandingItem = event.GetItem();
  wxTreeItemId firstChildItem = GetFirstChild(expandingItem, cookie);
  wxTreeItemData *itemData = GetItemData(firstChildItem);
  if (itemData == NULL) return;
  LazyLoader *lazyLoader = dynamic_cast<LazyLoader*>(itemData);
  if (lazyLoader != NULL) {
    lazyLoader->load(expandingItem);
    wxString expandingItemText = GetItemText(expandingItem);
    SetItemText(expandingItem, expandingItemText + _(" (loading...)"));
    Delete(firstChildItem);
    // veto expand event, for lazy loader to complete
    event.Veto();
  }
}

void ObjectBrowser::LoadDatabase(wxTreeItemId databaseItem, DatabaseModel *database) {
  SubmitDatabaseWork(database, new LoadDatabaseSchemaWork(this, database, databaseItem));
}

void ObjectBrowser::LoadRelation(wxTreeItemId relationItem, RelationModel *relation) {
  wxLogDebug(_T("Load data for relation %s.%s"), relation->schema.c_str(), relation->name.c_str());
  SubmitDatabaseWork(relation->database, new LoadRelationWork(this, relation, relationItem));
}

void ObjectBrowser::SubmitDatabaseWork(DatabaseModel *database, DatabaseWork *work) {
  DatabaseConnection *conn = database->server->conn->getConnection(database->name);
  // bodge
  if (!conn->IsConnected())
    conn->Connect();
  conn->AddWork(work);
}

void ObjectBrowser::FillInServer(ServerModel *serverModel, wxTreeItemId serverItem, wxString& serverVersion) {
  if (serverVersionRegex.Matches(serverVersion)) {
    serverModel->SetVersion(atoi(serverVersionRegex.GetMatch(serverVersion, 2).utf8_str()),
			    atoi(serverVersionRegex.GetMatch(serverVersion, 3).utf8_str()),
			    serverVersionRegex.GetMatch(serverVersion, 4));
    SetItemText(serverItem, serverModel->conn->Identification() + _T(" (") + serverVersionRegex.GetMatch(serverVersion, 1) + _T(")"));
  }
  else {
    SetItemText(serverItem, serverModel->conn->Identification());
  }
}

static bool collateDatabases(DatabaseModel *d1, DatabaseModel *d2) {
  return d1->name < d2->name;
}

void ObjectBrowser::FillInDatabases(ServerModel *serverModel, wxTreeItemId serverItem, vector<DatabaseModel*> &databases) {
  sort(databases.begin(), databases.end(), collateDatabases);
  vector<DatabaseModel*> systemDatabases;
  vector<DatabaseModel*> templateDatabases;
  vector<DatabaseModel*> userDatabases;
  wxTreeItemId systemDatabasesItem = NULL;
  wxTreeItemId templateDatabasesItem = NULL;
  for (vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
    DatabaseModel *database = *iter;
    if (database->IsSystem()) {
      systemDatabases.push_back(database);
    }
    else if (database->isTemplate) {
      templateDatabases.push_back(database);
    }
    else {
      userDatabases.push_back(database);
    }
  }
  if (!userDatabases.empty()) {
    AppendDatabaseItems(serverItem, userDatabases);
  }
  if (!templateDatabases.empty()) {
    wxTreeItemId templateDatabasesItem = AppendItem(serverItem, _("Template Databases"));
    AppendDatabaseItems(templateDatabasesItem, templateDatabases);
  }
  if (!systemDatabases.empty()) {
    wxTreeItemId systemDatabasesItem = AppendItem(serverItem, _("System Databases"));
    AppendDatabaseItems(systemDatabasesItem, systemDatabases);
  }
}

void ObjectBrowser::AppendDatabaseItems(wxTreeItemId parentItem, vector<DatabaseModel*> &databases) {
  for (vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
    DatabaseModel *database = *iter;
    wxTreeItemId databaseItem = AppendItem(parentItem, database->name);
    SetItemData(databaseItem, database);
    if (database->IsUsable())
      SetItemData(AppendItem(databaseItem, _T("Loading...")), new DatabaseLoader(this, database));
  }
}

static bool collateRoles(RoleModel *r1, RoleModel *r2) {
  return r1->name < r2->name;
}

void ObjectBrowser::FillInRoles(ServerModel *serverModel, wxTreeItemId serverItem, vector<RoleModel*> &roles) {
  sort(roles.begin(), roles.end(), collateRoles);
  wxTreeItemId usersItem = AppendItem(serverItem, _("Users"));
  wxTreeItemId groupsItem = AppendItem(serverItem, _("Groups"));
  for (vector<RoleModel*>::iterator iter = roles.begin(); iter != roles.end(); iter++) {
    RoleModel *role = *iter;
    wxTreeItemId roleItem;
    if (role->canLogin) {
      roleItem = AppendItem(usersItem, role->name);
    }
    else {
      roleItem = AppendItem(groupsItem, role->name);
    }
    SetItemData(roleItem, role);
  }
}

static bool collateSchemaMembers(SchemaMemberModel *r1, SchemaMemberModel *r2) {
  if (r1->schema < r2->schema) return true;
  if (r1->schema.IsSameAs(r2->schema)) {
    return r1->name < r2->name;
  }
  return false;
}

void ObjectBrowser::FillInDatabaseSchema(DatabaseModel *databaseModel, wxTreeItemId databaseItem, vector<RelationModel*> &relations, vector<FunctionModel*> &functions) {
  sort(relations.begin(), relations.end(), collateSchemaMembers);
  sort(functions.begin(), functions.end(), collateSchemaMembers);
  map<wxString, vector<SchemaMemberModel*> > systemSchemas;
  map<wxString, vector<SchemaMemberModel*> > userSchemas;

  int userObjectCount = 0;
  for (vector<RelationModel*>::iterator iter = relations.begin(); iter != relations.end(); iter++) {
    RelationModel *relation = *iter;
    if (relation->user) {
      userObjectCount++;
      userSchemas[relation->schema].push_back(relation);
    }
    else {
      systemSchemas[relation->schema].push_back(relation);
    }
  }
  for (vector<FunctionModel*>::iterator iter = functions.begin(); iter != functions.end(); iter++) {
    FunctionModel *function = *iter;
    if (function->user) {
      userObjectCount++;
      userSchemas[function->schema].push_back(function);
    }
    else {
      systemSchemas[function->schema].push_back(function);
    }
  }

  bool foldUserSchemas;
  if (userObjectCount < 50) {
    wxLogDebug(_T("Found %d user objects, unfolding schemas"), userObjectCount);
    foldUserSchemas = false;
  }
  else {
    wxLogDebug(_T("Found %d user objects, keeping all schemas folded"), userObjectCount);
    foldUserSchemas = true;
  }

  for (map<wxString, vector<SchemaMemberModel*> >::iterator iter = userSchemas.begin(); iter != userSchemas.end(); iter++) {
    AppendSchemaMembers(databaseItem, foldUserSchemas && iter->second.size() > 1, iter->first, iter->second);
  }

  wxTreeItemId systemSchemaMember = AppendItem(databaseItem, _("System schemas"));
  for (map<wxString, vector<SchemaMemberModel*> >::iterator iter = systemSchemas.begin(); iter != systemSchemas.end(); iter++) {
    AppendSchemaMembers(systemSchemaMember, iter->second.size() > 1, iter->first, iter->second);
  }
}

void ObjectBrowser::AppendSchemaMembers(wxTreeItemId parent, bool includeSchemaMember, const wxString &schemaName, vector<SchemaMemberModel*> &members) {
  if (members.size() == 1 && members[0]->name.IsEmpty()) {
    AppendItem(parent, schemaName + _T("."));
    return;
  }

  if (includeSchemaMember) {
    parent = AppendItem(parent, schemaName + _T("."));
  }

  wxTreeItemId functionsItem;
  bool schemaHasRelations = true;
  for (vector<SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
    SchemaMemberModel *member = *iter;
    if (member->name.IsEmpty()) {
      schemaHasRelations = false;
      continue;
    }
    // not sure if this is better or worse than having a virtual model method to have the model add itself
    RelationModel *relation = dynamic_cast<RelationModel*>(member);
    if (relation != NULL) {
      wxTreeItemId memberItem = AppendItem(parent, includeSchemaMember ? relation->name : relation->schema + _T(".") + relation->name);
      SetItemData(memberItem, relation);
      if (relation->type == RelationModel::TABLE || relation->type == RelationModel::VIEW)
	SetItemData(AppendItem(memberItem, _T("Loading...")), new RelationLoader(this, relation));
      continue;
    }
    FunctionModel *function = dynamic_cast<FunctionModel*>(member);
    if (function != NULL) {
      wxTreeItemId functionParent;
      if (includeSchemaMember && schemaHasRelations) {
	if (!functionsItem.IsOk())
	  functionsItem = AppendItem(parent, _("Functions"));
	functionParent = functionsItem;
      }
      else {
	functionParent = parent;
      }
      wxTreeItemId memberItem = AppendItem(functionParent, includeSchemaMember ? function->prototype : function->schema + _T(".") + function->prototype);
      SetItemData(memberItem, function);
      continue;
    }
  }
}

void ObjectBrowser::FillInRelation(RelationModel *relation, wxTreeItemId relationItem, vector<ColumnModel*> &columns, vector<IndexModel*> &indices, vector<TriggerModel*> &triggers) {
  for (vector<ColumnModel*>::iterator iter = columns.begin(); iter != columns.end(); iter++) {
    ColumnModel *column = *iter;
    wxString itemText = column->name + _T(" (") + column->type;

    if (relation->type == RelationModel::TABLE) {
      if (column->nullable)
	itemText += _T(", null");
      else
	itemText += _T(", not null");
      if (column->hasDefault)
	itemText += _T(", default");
    }

    itemText += _T(")");

    wxTreeItemId columnItem = AppendItem(relationItem, itemText);
    SetItemData(columnItem, column);
  }

  if (!indices.empty()) {
    wxTreeItemId indicesItem = AppendItem(relationItem, _("Indices"));
    for (vector<IndexModel*>::iterator iter = indices.begin(); iter != indices.end(); iter++) {
      wxTreeItemId indexItem = AppendItem(indicesItem, (*iter)->name);
      SetItemData(indexItem, *iter);
    }
  }

  if (!triggers.empty()) {
    wxTreeItemId triggersItem = AppendItem(relationItem, _("Triggers"));
    for (vector<TriggerModel*>::iterator iter = triggers.begin(); iter != triggers.end(); iter++) {
      wxTreeItemId triggerItem = AppendItem(triggersItem, (*iter)->name);
      SetItemData(triggerItem, *iter);
    }
  }
}

void ObjectBrowser::OnGetTooltip(wxTreeEvent &event) {
  wxTreeItemId item = event.GetItem();
  wxTreeItemData *itemData = GetItemData(item);
  if (itemData == NULL) return;
  ObjectModel *object = dynamic_cast<ObjectModel*>(itemData);
  if (object == NULL) return;
  event.SetToolTip(object->description);
}
