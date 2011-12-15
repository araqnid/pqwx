#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/regex.h"
#include <algorithm>

#include "object_browser.h"
#include "pqwx_frame.h"

const int EVENT_WORK_FINISHED = 10000;

BEGIN_EVENT_TABLE(ObjectBrowser, wxTreeCtrl)
  EVT_TREE_ITEM_EXPANDING(Pqwx_ObjectBrowser, ObjectBrowser::BeforeExpand)
  EVT_COMMAND(EVENT_WORK_FINISHED, wxEVT_COMMAND_TEXT_UPDATED, ObjectBrowser::OnWorkFinished)
END_EVENT_TABLE()

typedef std::vector< std::vector<wxString> > QueryResults;

static wxRegEx serverVersionRegex(wxT("^PostgreSQL ([0-9]+)\\.([0-9]+)([0-9.a-z]*)"));

static bool ExecQuerySync(PGconn *conn, const char *sql, QueryResults& results) {
  PGresult *rs = PQexec(conn, sql);
  if (!rs)
    return false;

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_TUPLES_OK)
    return false; // expected data back

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

  PQclear(rs);

  return true;
}

class ColumnModel : public wxTreeItemData {
public:
  RelationModel *relation;
  wxString name;
  wxString type;
  bool nullable;
  bool hasDefault;
};

class RelationModel : public wxTreeItemData {
public:
  DatabaseModel *database;
  unsigned long oid;
  wxString schema;
  wxString name;
  bool user;
  enum { TABLE, VIEW, SEQUENCE } type;
};

class DatabaseModel : public wxTreeItemData {
public:
  unsigned long oid;
  bool isTemplate;
  bool allowConnections;
  bool havePrivsToConnect;
  wxString name;
  ServerModel *server;
  bool IsUsable() {
    return allowConnections && havePrivsToConnect;
  }
  bool IsSystem() {
    return name.IsSameAs(_T("postgres")) || name.IsSameAs(_T("template0")) || name.IsSameAs(_T("template1"));
  }
};

class RoleModel : public wxTreeItemData {
public:
  unsigned long oid;
  bool isSuperuser;
  bool canLogin;
  wxString name;
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

#define GET_OID(iter, index, result) (*iter)[index].ToULong(&(result))
#define GET_BOOLEAN(iter, index, result) result = (*iter)[index].IsSameAs(_T("t"))
#define GET_TEXT(iter, index, result) result = (*iter)[index]

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
    doQuery(conn, "SELECT version()", serverRows);
    serverVersion = serverRows[0][0];
  }
  void loadDatabases(PGconn *conn) {
    QueryResults databaseRows;
    doQuery(conn, "SELECT oid, datname, datistemplate, datallowconn, has_database_privilege(oid, 'connect') AS can_connect FROM pg_database", databaseRows);
    for (QueryResults::iterator iter = databaseRows.begin(); iter != databaseRows.end(); iter++) {
      DatabaseModel *database = new DatabaseModel();
      database->server = serverModel;
      GET_OID(iter, 0, database->oid);
      GET_TEXT(iter, 1, database->name);
      GET_BOOLEAN(iter, 2, database->isTemplate);
      GET_BOOLEAN(iter, 3, database->allowConnections);
      GET_BOOLEAN(iter, 4, database->havePrivsToConnect);
      databases.push_back(database);
    }
  }
  void loadRoles(PGconn *conn) {
    QueryResults roleRows;
    doQuery(conn, "SELECT oid, rolname, rolcanlogin, rolsuper FROM pg_roles", roleRows);
    for (QueryResults::iterator iter = roleRows.begin(); iter != roleRows.end(); iter++) {
      RoleModel *role = new RoleModel();
      GET_OID(iter, 0, role->oid);
      GET_TEXT(iter, 1, role->name);
      GET_BOOLEAN(iter, 2, role->canLogin);
      GET_BOOLEAN(iter, 3, role->isSuperuser);
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
protected:
  void executeInTransaction(PGconn *conn) {
    QueryResults relationRows;
    doQuery(conn, "SELECT pg_class.oid, nspname, relname, relkind FROM (SELECT oid, relname, relkind, relnamespace FROM pg_class WHERE relkind IN ('r','v') OR (relkind = 'S' AND NOT EXISTS (SELECT 1 FROM pg_depend WHERE classid = 'pg_class'::regclass AND objid = pg_class.oid AND refclassid = 'pg_class'::regclass AND deptype = 'a'))) pg_class RIGHT JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace", relationRows);
    for (QueryResults::iterator iter = relationRows.begin(); iter != relationRows.end(); iter++) {
      RelationModel *relation = new RelationModel();
      relation->database = databaseModel;
      (*iter)[0].ToULong(&(relation->oid));
      relation->schema = (*iter)[1];
      relation->name = (*iter)[2];
      wxString relkind = (*iter)[3];
      if (relkind.IsSameAs(_("r")))
	relation->type = RelationModel::TABLE;
      else if (relkind.IsSameAs(_("v")))
	relation->type = RelationModel::VIEW;
      else if (relkind.IsSameAs(_("S")))
	relation->type = RelationModel::SEQUENCE;
      relation->user = !systemSchema(relation->schema);
      relations.push_back(relation);
    }
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    ob->FillInDatabaseSchema(databaseModel, databaseItem, relations);
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
protected:
  void executeInTransaction(PGconn *conn) {
    QueryResults attributeRows;
    wxString oidValue;
    oidValue.Printf(_T("%d"), relationModel->oid);
    doQuery(conn, "SELECT attname, pg_catalog.format_type(atttypid, atttypmod), NOT attnotnull, atthasdef FROM pg_attribute WHERE attrelid = $1 AND NOT attisdropped AND attnum > 0 ORDER BY attnum", attributeRows, 23 /* int4 */, oidValue.utf8_str());
    for (QueryResults::iterator iter = attributeRows.begin(); iter != attributeRows.end(); iter++) {
      ColumnModel *column = new ColumnModel();
      column->relation = relationModel;
      GET_TEXT(iter, 0, column->name);
      GET_TEXT(iter, 1, column->type);
      GET_BOOLEAN(iter, 2, column->nullable);
      GET_BOOLEAN(iter, 3, column->hasDefault);
      columns.push_back(column);
    }
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    ob->FillInRelation(relationModel, relationItem, columns);
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

static wxString nameOf(ServerConnection *conn) {
  wxString serverItemName;
  if (conn->username != NULL) {
    serverItemName << wxString(conn->username, wxConvUTF8) << wxT('@');
  }
  if (conn->hostname != NULL) {
    serverItemName << wxString(conn->hostname, wxConvUTF8);
  }
  else {
    serverItemName << _("[local]");
  }
  if (conn->port > 0) {
    serverItemName << wxT(":") << wxString::Format(wxT("%d"), conn->port);
  }
  return serverItemName;
}

void ObjectBrowser::AddServerConnection(ServerConnection *conn) {
  ServerModel *serverModel = new ServerModel();
  serverModel->conn = conn;
  servers.push_back(serverModel);

  // setting the text twice is a bug workaround for wx 2.8
  // see http://trac.wxwidgets.org/ticket/10085
  wxString connName = nameOf(conn);
  wxLogDebug(_T("Connection added to object browser: %s"), connName.c_str());
  wxTreeItemId serverItem = AppendItem(GetRootItem(), connName);
  SetItemText(serverItem, connName);
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
    serverModel->SetVersion(atoi(serverVersionRegex.GetMatch(serverVersion, 1).utf8_str()),
			    atoi(serverVersionRegex.GetMatch(serverVersion, 2).utf8_str()),
			    serverVersionRegex.GetMatch(serverVersion, 3));
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

static bool collateRelations(RelationModel *r1, RelationModel *r2) {
  if (r1->schema < r2->schema) return true;
  if (r1->schema.IsSameAs(r2->schema)) {
    return r1->name < r2->name;
  }
  return false;
}

void ObjectBrowser::FillInDatabaseSchema(DatabaseModel *databaseModel, wxTreeItemId databaseItem, vector<RelationModel*> &relations) {
  sort(relations.begin(), relations.end(), collateRelations);
  map<wxString, vector<RelationModel*> > systemSchemas;
  map<wxString, vector<RelationModel*> > userSchemas;

  int userRelationCount = 0;
  for (vector<RelationModel*>::iterator iter = relations.begin(); iter != relations.end(); iter++) {
    RelationModel *relation = *iter;
    if (relation->user) {
      userRelationCount++;
      userSchemas[relation->schema].push_back(relation);
    }
    else {
      systemSchemas[relation->schema].push_back(relation);
    }
  }

  if (userRelationCount < 50) {
    wxLogDebug(_T("Found %d user relations, unfolding schemas"), userRelationCount);
    // unfold user schemas
    for (map<wxString, vector<RelationModel*> >::iterator iter = userSchemas.begin(); iter != userSchemas.end(); iter++) {
      AppendSchemaItems(databaseItem, false, iter->first, iter->second);
    }
  }
  else {
    wxLogDebug(_T("Found %d user relations, keeping all schemas folded"), userRelationCount);
    for (map<wxString, vector<RelationModel*> >::iterator iter = userSchemas.begin(); iter != userSchemas.end(); iter++) {
      AppendSchemaItems(databaseItem, iter->second.size() > 1, iter->first, iter->second);
    }
  }

  wxTreeItemId systemSchemasItem = AppendItem(databaseItem, _("System schemas"));
  for (map<wxString, vector<RelationModel*> >::iterator iter = systemSchemas.begin(); iter != systemSchemas.end(); iter++) {
    AppendSchemaItems(systemSchemasItem, true, iter->first, iter->second);
  }
}

void ObjectBrowser::AppendSchemaItems(wxTreeItemId parent, bool includeSchemaItem, const wxString &schemaName, vector<RelationModel*> &relations) {
  if (relations.size() == 1 && relations[0]->name.IsEmpty()) {
    AppendItem(parent, schemaName + _T("."));
    return;
  }

  if (includeSchemaItem) {
    parent = AppendItem(parent, schemaName + _T("."));
  }

  for (vector<RelationModel*>::iterator iter = relations.begin(); iter != relations.end(); iter++) {
    RelationModel *relation = *iter;
    wxTreeItemId relationItem = AppendItem(parent, includeSchemaItem ? relation->name : relation->schema + _T(".") + relation->name);
    SetItemData(relationItem, relation);
    if (relation->type == RelationModel::TABLE || relation->type == RelationModel::VIEW)
      SetItemData(AppendItem(relationItem, _T("Loading...")), new RelationLoader(this, relation));
  }
}

void ObjectBrowser::FillInRelation(RelationModel *relation, wxTreeItemId relationItem, vector<ColumnModel*> &columns) {
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
}
