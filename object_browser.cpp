#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <algorithm>
#include <set>

#include "object_browser.h"
#include "object_browser_sql.h"
#include "object_finder.h"
#include "pqwx_frame.h"
#include "catalogue_index.h"

const int EVENT_WORK_FINISHED = 10000;

BEGIN_EVENT_TABLE(ObjectBrowser, wxTreeCtrl)
  EVT_TREE_ITEM_EXPANDING(Pqwx_ObjectBrowser, ObjectBrowser::BeforeExpand)
  EVT_TREE_ITEM_GETTOOLTIP(Pqwx_ObjectBrowser, ObjectBrowser::OnGetTooltip)
  EVT_COMMAND(EVENT_WORK_FINISHED, wxEVT_COMMAND_TEXT_UPDATED, ObjectBrowser::OnWorkFinished)
END_EVENT_TABLE()

typedef std::vector< std::vector<wxString> > QueryResults;

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
  bool loaded;
  ServerModel *server;
  CatalogueIndex *catalogueIndex;
  bool IsUsable() {
    return allowConnections && havePrivsToConnect;
  }
  bool IsSystem() {
    return name.IsSameAs(_T("postgres")) || name.IsSameAs(_T("template0")) || name.IsSameAs(_T("template1"));
  }
  map<unsigned long,wxTreeItemId> symbolItemLookup;
};

class RoleModel : public ObjectModel {
public:
  unsigned long oid;
  bool superuser;
  bool canLogin;
};

class ServerModel : public wxTreeItemData {
public:
  ServerModel() {}
  ServerModel(DatabaseConnection *db) {
    connections[db->DbName()] = db;
    db->Relabel(_("Object Browser"));
  }
  ServerConnection *conn;
  vector<DatabaseModel*> databases;
  int serverVersion;
  bool usingSSL;
  map<wxString, DatabaseConnection*> connections;
  void SetVersion(int version) {
    serverVersion = version;
  }
  DatabaseModel *findDatabase(unsigned long oid) {
    for (vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
      if ((*iter)->oid == oid)
	return *iter;
    }
    return NULL;
  }
  bool versionNotBefore(int major, int minor) {
    return serverVersion >= (major * 10000 + minor * 100);
  }
  void Dispose() {
    wxLogDebug(_T("Disposing of server %s"), conn->Identification().c_str());
    for (map<wxString, DatabaseConnection*>::iterator iter = connections.begin(); iter != connections.end(); iter++) {
      DatabaseConnection *db = iter->second;
      wxLogDebug(_T(" Closing connection to %s"), iter->first.c_str());
      db->CloseSync();
    }
    connections.clear();
  }
};

class ObjectBrowserWork : public DatabaseWork {
public:
  ObjectBrowserWork(ObjectBrowser *owner) : owner(owner) {}
  virtual ~ObjectBrowserWork() {}
  void execute(PGconn *conn) {
    if (PQserverVersion(conn) >= 80000) {
      if (!doCommand(conn, "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY"))
	return;
    }
    else {
      if (!doCommand(conn, "BEGIN"))
	return;
      if (!doCommand(conn, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE"))
	return;
      if (!doCommand(conn, "SET TRANSACTION READ ONLY"))
	return;
    }

    executeInTransaction(conn);

    doCommand(conn, "END");
  }
  virtual void loadResultsToGui(ObjectBrowser *browser) = 0;
protected:
  void notifyFinished() {
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_WORK_FINISHED);
    event.SetClientData(this);
    owner->AddPendingEvent(event);
  }
  virtual void executeInTransaction(PGconn *conn) = 0;
  bool doCommand(PGconn *conn, const char *sql) {
    return cmd(conn, sql);
  }
  bool doQuery(PGconn *conn, const wxString &name, QueryResults &results, Oid paramType, const char *paramValue) {
    doQuery(conn, GetSql(conn, name), results, paramType, paramValue);
  }
  bool doQuery(PGconn *conn, const wxString &name, QueryResults &results) {
    doQuery(conn, GetSql(conn, name), results);
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
  ObjectBrowser *owner;
  const char *GetSql(PGconn *conn, const wxString &name) const {
    return owner->GetSql(name, PQserverVersion(conn));
  }
};

#define CHECK_INDEX(iter, index) wxASSERT(index < (*iter).size())
#define GET_OID(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToULong(&(result))
#define GET_BOOLEAN(iter, index, result) CHECK_INDEX(iter, index); result = (*iter)[index].IsSameAs(_T("t"))
#define GET_TEXT(iter, index, result) CHECK_INDEX(iter, index); result = (*iter)[index]
#define GET_INT4(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToLong(&(result))
#define GET_INT8(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToLong(&(result))

class RefreshDatabaseListWork : public ObjectBrowserWork {
public:
  RefreshDatabaseListWork(ObjectBrowser *owner, ServerModel *serverModel, wxTreeItemId serverItem) : ObjectBrowserWork(owner), serverModel(serverModel), serverItem(serverItem) {
    wxLogDebug(_T("%p: work to load database list"), this);
  }
protected:
  void executeInTransaction(PGconn *conn) {
    loadServer(conn);
    loadDatabases(conn);
    loadRoles(conn);
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    ob->FillInServer(serverModel, serverItem, serverVersionString, serverVersion, usingSSL);
    ob->FillInDatabases(serverModel, serverItem, databases);
    ob->FillInRoles(serverModel, serverItem, roles);

    ob->Expand(serverItem);
  }
private:
  ServerModel *serverModel;
  wxTreeItemId serverItem;
  vector<DatabaseModel*> databases;
  vector<RoleModel*> roles;
  wxString serverVersionString;
  int serverVersion;
  bool usingSSL;
  void loadServer(PGconn *conn) {
    const char *serverVersionRaw = PQparameterStatus(conn, "server_version");
    serverVersionString = wxString(serverVersionRaw, wxConvUTF8);
    serverVersion = PQserverVersion(conn);
    void *ssl = PQgetssl(conn);
    usingSSL = (ssl != NULL);
  }
  void loadDatabases(PGconn *conn) {
    QueryResults databaseRows;
    doQuery(conn, _T("Databases"), databaseRows);
    for (QueryResults::iterator iter = databaseRows.begin(); iter != databaseRows.end(); iter++) {
      DatabaseModel *database = new DatabaseModel();
      database->server = serverModel;
      database->loaded = false;
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
    doQuery(conn, _T("Roles"), roleRows);
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
  LoadDatabaseSchemaWork(ObjectBrowser *owner, DatabaseModel *databaseModel, wxTreeItemId databaseItem) : ObjectBrowserWork(owner), databaseModel(databaseModel), databaseItem(databaseItem) {
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
    doQuery(conn, _T("Relations"), relationRows);
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
    doQuery(conn, _T("Functions"), functionRows);
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

class IndexDatabaseSchemaWork : public ObjectBrowserWork {
public:
  IndexDatabaseSchemaWork(ObjectBrowser *owner, DatabaseModel *database) : ObjectBrowserWork(owner), database(database) {
    wxLogDebug(_T("%p: work to index schema"), this);
  }
private:
  DatabaseModel *database;
  CatalogueIndex *catalogueIndex;
protected:
  void executeInTransaction(PGconn *conn) {
    map<wxString, CatalogueIndex::Type> typeMap;
    typeMap[_T("t")] = CatalogueIndex::TABLE;
    typeMap[_T("v")] = CatalogueIndex::VIEW;
    typeMap[_T("s")] = CatalogueIndex::SEQUENCE;
    typeMap[_T("f")] = CatalogueIndex::FUNCTION_SCALAR;
    typeMap[_T("fs")] = CatalogueIndex::FUNCTION_ROWSET;
    typeMap[_T("ft")] = CatalogueIndex::FUNCTION_TRIGGER;
    typeMap[_T("fa")] = CatalogueIndex::FUNCTION_AGGREGATE;
    typeMap[_T("fw")] = CatalogueIndex::FUNCTION_WINDOW;
    typeMap[_T("T")] = CatalogueIndex::TYPE;
    typeMap[_T("x")] = CatalogueIndex::EXTENSION;
    typeMap[_T("O")] = CatalogueIndex::COLLATION;
    QueryResults rs;
    doQuery(conn, _T("IndexSchema"), rs);
    catalogueIndex = new CatalogueIndex();
    catalogueIndex->Begin();
    for (QueryResults::iterator iter = rs.begin(); iter != rs.end(); iter++) {
      long entityId;
      wxString typeString;
      wxString symbol;
      wxString disambig;
      GET_INT8(iter, 0, entityId);
      GET_TEXT(iter, 1, typeString);
      GET_TEXT(iter, 2, symbol);
      GET_TEXT(iter, 3, disambig);
      bool systemObject;
      CatalogueIndex::Type entityType;
      if (typeString.Last() == _T('S')) {
	systemObject = true;
	typeString.RemoveLast();
	wxASSERT(typeMap.count(typeString) > 0);
	entityType = typeMap[typeString];
      }
      else {
	systemObject = false;
	wxASSERT(typeMap.count(typeString) > 0);
	entityType = typeMap[typeString];
      }
      catalogueIndex->AddDocument(CatalogueIndex::Document(entityId, entityType, systemObject, symbol, disambig));
    }
    catalogueIndex->Commit();
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    database->catalogueIndex = catalogueIndex;
  }
};

class LoadRelationWork : public ObjectBrowserWork {
public:
  LoadRelationWork(ObjectBrowser *owner, RelationModel *relationModel, wxTreeItemId relationItem) : ObjectBrowserWork(owner), relationModel(relationModel), relationItem(relationItem) {
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
    doQuery(conn, _T("Columns"), attributeRows, 26 /* oid */, oidValue.utf8_str());
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
    doQuery(conn, _T("Indices"), indexRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = indexRows.begin(); iter != indexRows.end(); iter++) {
      IndexModel *index = new IndexModel();
      GET_TEXT(iter, 0, index->name);
      indices.push_back(index);
    }
  }
  void loadTriggers(PGconn *conn) {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults triggerRows;
    doQuery(conn, _T("Triggers"), triggerRows, 26 /* oid */, oidValue.utf8_str());
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

ObjectBrowser::ObjectBrowser(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style) : wxTreeCtrl(parent, id, pos, size, style), sql() {
  AddRoot(_T("root"));
  sql = new ObjectBrowserSql();
}

void ObjectBrowser::AddServerConnection(ServerConnection *server, DatabaseConnection *db) {
  wxString serverId = server->Identification();
  for (list<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    ServerModel *serverModel = *iter;
    if (serverModel->conn->Identification().IsSameAs(serverId)) {
      wxLogDebug(_T("Ignoring server connection already registered in object browser: %s"), serverId.c_str());
      if (db != NULL) {
	db->CloseSync();
	delete db;
      }
      return;
    }
  }

  ServerModel *serverModel = db ? new ServerModel(db) : new ServerModel();
  serverModel->conn = server;
  servers.push_back(serverModel);

  // setting the text twice is a bug workaround for wx 2.8
  // see http://trac.wxwidgets.org/ticket/10085
  wxLogDebug(_T("Connection added to object browser: %s"), server->Identification().c_str());
  wxTreeItemId serverItem = AppendItem(GetRootItem(), server->Identification());
  SetItemText(serverItem, server->Identification());
  SetItemData(serverItem, serverModel);

  RefreshDatabaseList(serverItem);
}

DatabaseConnection *ObjectBrowser::GetServerAdminConnection(ServerModel *server) {
  return GetDatabaseConnection(server, server->conn->globalDbName);
}

DatabaseConnection *ObjectBrowser::GetDatabaseConnection(ServerModel *server, const wxString &dbname) {
  DatabaseConnection *db = server->connections[dbname];
  if (db == NULL) {
    wxLogDebug(_T("Allocating connection to %s database %s"), server->conn->Identification().c_str(), dbname.c_str());
    db = new DatabaseConnection(server->conn, dbname, _("Object Browser"));
    server->connections[dbname] = db;
  }
  return db;
}

void ObjectBrowser::Dispose() {
  wxLogDebug(_T("Disposing of ObjectBrowser"));
  for (list<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    ServerModel *server = *iter;
    server->Dispose();
  }
}

void ObjectBrowser::RefreshDatabaseList(wxTreeItemId serverItem) {
  ServerModel *serverModel = dynamic_cast<ServerModel*>(GetItemData(serverItem));
  SubmitServerWork(serverModel, new RefreshDatabaseListWork(this, serverModel, serverItem));
}

void ObjectBrowser::SubmitServerWork(ServerModel *serverModel, DatabaseWork *work) {
  ConnectAndAddWork(serverModel, GetServerAdminConnection(serverModel), work);
}

void ObjectBrowser::ConnectAndAddWork(ServerModel *serverModel, DatabaseConnection *db, DatabaseWork *work) {
  // still a bodge. what if the database connection fails? need to clean up any work added in the meantime...
  if (!db->IsConnected()) {
    db->Connect();
  }
  db->AddWork(work);
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
  SubmitDatabaseWork(database, new IndexDatabaseSchemaWork(this, database));
}

void ObjectBrowser::LoadRelation(wxTreeItemId relationItem, RelationModel *relation) {
  wxLogDebug(_T("Load data for relation %s.%s"), relation->schema.c_str(), relation->name.c_str());
  SubmitDatabaseWork(relation->database, new LoadRelationWork(this, relation, relationItem));
}

void ObjectBrowser::SubmitDatabaseWork(DatabaseModel *database, DatabaseWork *work) {
  ConnectAndAddWork(database->server, GetDatabaseConnection(database->server, database->name), work);
}

void ObjectBrowser::FillInServer(ServerModel *serverModel, wxTreeItemId serverItem, const wxString &serverVersionString, int serverVersion, bool usingSSL) {
  serverModel->SetVersion(serverVersion);
  serverModel->usingSSL = usingSSL;
  SetItemText(serverItem, serverModel->conn->Identification() + _T(" (") + serverVersionString + _T(")") + (usingSSL ? _T(" [SSL]") : wxEmptyString));
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
  databaseModel->loaded = true;

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
      relation->database->symbolItemLookup[relation->oid] = memberItem;
      if (relation->type == RelationModel::TABLE || relation->type == RelationModel::VIEW)
	SetItemData(AppendItem(memberItem, _("Loading...")), new RelationLoader(this, relation));
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
      function->database->symbolItemLookup[function->oid] = memberItem;
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
	itemText += _(", null");
      else
	itemText += _(", not null");
      if (column->hasDefault)
	itemText += _(", default");
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

void ObjectBrowser::DisconnectSelected() {
  wxTreeItemId selected = GetSelection();
  if (!selected.IsOk()) {
    wxLogDebug(_T("selected item was not ok -- nothing is selected?"));
    return;
  }
  if (selected == GetRootItem()) {
    wxLogDebug(_T("selected item was root -- nothing is selected"));
    return;
  }
  wxTreeItemId cursor = selected;
  do {
    wxTreeItemId parent = GetItemParent(cursor);
    if (parent == GetRootItem())
      break;
    cursor = parent;
  } while (1);

  wxTreeItemData *data = GetItemData(cursor);
  wxASSERT(data != NULL);

  // in one of the top-level items, we'd better have a server model
  ServerModel *server = dynamic_cast<ServerModel*>(data);
  wxASSERT(server != NULL);

  wxLogDebug(_T("Disconnect: %s"), server->conn->Identification().c_str());
  servers.remove(server);
  server->Dispose(); // still does nasty synchronous disconnect for now
  Delete(cursor);
}

class ZoomToFoundObjectOnCompletion : public ObjectFinder::Completion {
public:
  ZoomToFoundObjectOnCompletion(ObjectBrowser *ob, DatabaseModel *database) : ob(ob), database(database) {}
  void OnObjectChosen(const CatalogueIndex::Document *document) {
    ob->ZoomToFoundObject(database, document);
  }
  void OnCancelled() {
  }
private:
  DatabaseModel *database;
  ObjectBrowser *ob;
};

void ObjectBrowser::FindObject() {
  wxTreeItemId selected = GetSelection();
  if (!selected.IsOk()) {
    wxLogDebug(_T("selected item was not ok -- nothing is selected?"));
    return;
  }
  if (selected == GetRootItem()) {
    wxLogDebug(_T("selected item was root -- nothing is selected"));
    return;
  }
  wxTreeItemId cursor = selected;
  wxTreeItemId previous;
  do {
    wxTreeItemId parent = GetItemParent(cursor);
    if (parent == GetRootItem())
      break;
    previous = cursor;
    cursor = parent;
  } while (1);
  if (!previous.IsOk()) {
    wxLogDebug(_T("selection is above database level?"));
    return;
  }

  wxTreeItemData *data = GetItemData(previous);
  wxASSERT(data != NULL);

  DatabaseModel *database = dynamic_cast<DatabaseModel*>(data);
  wxASSERT(database != NULL);

  wxLogDebug(_T("Find object in %s %s"), database->server->conn->Identification().c_str(), database->name.c_str());

  // TODO - should be able to open the dialogue and have it start loading this if it's not already done
  if (!database->loaded) {
    wxMessageBox(_T("Database not loaded yet (expand the tree node)"));
    return;
  }
  wxASSERT(database->catalogueIndex != NULL);

  ObjectFinder *finder = new ObjectFinder(NULL, new ZoomToFoundObjectOnCompletion(this, database), database->catalogueIndex);
  finder->Show();
  finder->SetFocus();
}

void ObjectBrowser::ZoomToFoundObject(DatabaseModel *database, const CatalogueIndex::Document *document) {
  wxLogDebug(_T("Zoom to found object \"%s\" in database \"%s\" of \"%s\""), document->symbol.c_str(), database->name.c_str(), database->server->conn->Identification().c_str());
  wxASSERT(database->symbolItemLookup.count(document->entityId) > 0);
  wxTreeItemId item = database->symbolItemLookup[document->entityId];
  EnsureVisible(item);
  SelectItem(item);
  Expand(item);
}
