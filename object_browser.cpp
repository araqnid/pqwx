#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/toolbar.h"

#include "object_browser.h"
#include "pqwx_frame.h"

const int EVENT_WORK_FINISHED = 10000;

BEGIN_EVENT_TABLE(ObjectBrowser, wxTreeCtrl)
  EVT_TREE_ITEM_EXPANDING(Pqwx_ObjectBrowser, ObjectBrowser::BeforeExpand)
  EVT_COMMAND(EVENT_WORK_FINISHED, wxEVT_COMMAND_TEXT_UPDATED, ObjectBrowser::OnWorkFinished)
END_EVENT_TABLE()

typedef std::vector< std::vector<wxString> > QueryResults;

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

class ObjectBrowserWork : public DatabaseWork {
public:
  ObjectBrowserWork(wxEvtHandler *owner) : dest(owner) {}
  virtual ~ObjectBrowserWork() {}
  void execute(PGconn *conn) {
    if (!cmd(conn, "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY"))
      return;

    executeInTransaction(conn);

    cmd(conn, "END");
  }
  virtual void loadResultsToGui(ObjectBrowser *browser) = 0;
protected:
  void notifyFinished() {
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_WORK_FINISHED);
    event.SetClientData(this);
    dest->AddPendingEvent(event);
  }
  virtual void executeInTransaction(PGconn *conn) = 0;
  bool cmd(PGconn *conn, const char *sql) {
    DatabaseCommandWork work(sql);
    work.execute(conn);
    return work.successful;
  }
  bool doQuery(PGconn *conn, const char *sql, QueryResults &results) {
    logSql(sql);

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
  ~RefreshDatabaseListWork() {
    wxLogDebug(_T("%p: deallocating work"), this);
  }
private:
  ServerModel *serverModel;
  wxTreeItemId serverItem;
  QueryResults databaseRows;
  QueryResults tablespaceRows;
  QueryResults roleRows;
protected:
  void executeInTransaction(PGconn *conn) {
    doQuery(conn, "SELECT oid, datname, datistemplate, datallowconn, has_database_privilege(oid, 'connect') AS can_connect FROM pg_database", databaseRows);
    doQuery(conn, "SELECT oid, spcname FROM pg_tablespace", tablespaceRows);
    doQuery(conn, "SELECT oid, rolname, rolcanlogin, rolsuper FROM pg_roles", roleRows);
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    wxLogDebug(_T("%p: loading database list into GUI"), this);

    vector<DatabaseModel*> userDatabases;
    vector<DatabaseModel*> templateDatabases;
    vector<DatabaseModel*> systemDatabases;
    vector<TablespaceModel*> userTablespaces;
    vector<TablespaceModel*> systemTablespaces;

    for (QueryResults::iterator iter = databaseRows.begin(); iter != databaseRows.end(); iter++) {
      DatabaseModel *databaseModel = new DatabaseModel();
      databaseModel->server = serverModel->conn;
      GET_OID(iter, 0, databaseModel->oid);
      GET_TEXT(iter, 1, databaseModel->name);
      GET_BOOLEAN(iter, 2, databaseModel->isTemplate);
      GET_BOOLEAN(iter, 3, databaseModel->allowConnections);
      GET_BOOLEAN(iter, 4, databaseModel->havePrivsToConnect);
      serverModel->databases.push_back(databaseModel);
      if (databaseModel->name.IsSameAs(_T("postgres"))
	  || databaseModel->name.IsSameAs(_T("template0"))
	  || databaseModel->name.IsSameAs(_T("template1"))) {
	systemDatabases.push_back(databaseModel);
      }
      else if (databaseModel->isTemplate) {
	templateDatabases.push_back(databaseModel);
      }
      else {
	userDatabases.push_back(databaseModel);
      }
    }

    for (QueryResults::iterator iter = tablespaceRows.begin(); iter != tablespaceRows.end(); iter++) {
      TablespaceModel *tablespaceModel = new TablespaceModel();
      GET_OID(iter, 0, tablespaceModel->oid);
      GET_TEXT(iter, 1, tablespaceModel->name);
      if (tablespaceModel->name.StartsWith(_T("pg_"))) {
	systemTablespaces.push_back(tablespaceModel);
      }
      else {
	userTablespaces.push_back(tablespaceModel);
      }
    }

    for (vector<DatabaseModel*>::iterator iter = userDatabases.begin(); iter != userDatabases.end(); iter++) {
      ob->AddDatabaseItem(serverItem, *iter);
    }

    for (vector<TablespaceModel*>::iterator iter = userTablespaces.begin(); iter != userTablespaces.end(); iter++) {
      ob->AddTablespaceItem(serverItem, *iter);
    }

    ob->AddSystemItems(serverItem);

    for (QueryResults::iterator iter = roleRows.begin(); iter != roleRows.end(); iter++) {
      int canLogin;
      GET_BOOLEAN(iter, 2, canLogin);
      if (canLogin) {
	UserModel *userModel = new UserModel();
	GET_OID(iter, 0, userModel->oid);
	GET_TEXT(iter, 1, userModel->name);
	GET_BOOLEAN(iter, 3, userModel->isSuperuser);
	ob->AddUserItem(serverItem, userModel);
      }
      else {
	GroupModel *groupModel = new GroupModel();
	GET_OID(iter, 0, groupModel->oid);
	GET_TEXT(iter, 1, groupModel->name);
	ob->AddGroupItem(serverItem, groupModel);
      }
    }

    for (vector<DatabaseModel*>::iterator iter = systemDatabases.begin(); iter != systemDatabases.end(); iter++) {
      ob->AddSystemDatabaseItem(serverItem, *iter);
    }

    for (vector<TablespaceModel*>::iterator iter = systemTablespaces.begin(); iter != systemTablespaces.end(); iter++) {
      ob->AddSystemTablespaceItem(serverItem, *iter);
    }

    wxLogDebug(_T("%p: finished loading database list"), this);

    ob->LoadedServer(serverItem);
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
  LoadDatabaseSchemaWork(wxEvtHandler *owner, DatabaseModel *databaseModel, wxTreeItemId databaseItemId) : ObjectBrowserWork(owner), databaseStubModel(databaseModel), databaseItemId(databaseItemId) {
    wxLogDebug(_T("%p: work to load schema"), this);
  }
  virtual ~LoadDatabaseSchemaWork() {
    wxLogDebug(_T("%p: deallocating work"), this);
  }
private:
  DatabaseModel *databaseStubModel;
  wxTreeItemId databaseItemId;
  QueryResults relations;
protected:
  void executeInTransaction(PGconn *conn) {
    doQuery(conn, "SELECT pg_class.oid, nspname, relname, relkind FROM (SELECT oid, relname, relkind, relnamespace FROM pg_class WHERE relkind IN ('r','v') OR (relkind = 'S' AND NOT EXISTS (SELECT 1 FROM pg_depend WHERE classid = 'pg_class'::regclass AND objid = pg_class.oid AND refclassid = 'pg_class'::regclass AND deptype = 'a'))) pg_class RIGHT JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace", relations);
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    wxLogDebug(_T("%p: loading database schema into GUI"), this);

    map<wxString, vector<RelationModel*> > relationMap;
    vector<RelationModel*> userRelations;
    vector<RelationModel*> systemRelations;

    int userRelationCount = 0;
    for (QueryResults::iterator iter = relations.begin(); iter != relations.end(); iter++) {
      RelationModel *model = new RelationModel();
      (*iter)[0].ToULong(&(model->oid));
      model->schema = (*iter)[1];
      model->name = (*iter)[2];
      wxString relkind = (*iter)[3];
      if (relkind.IsSameAs(_("r")))
	model->type = RelationModel::TABLE;
      else if (relkind.IsSameAs(_("v")))
	model->type = RelationModel::VIEW;
      else if (relkind.IsSameAs(_("S")))
	model->type = RelationModel::SEQUENCE;
      model->user = !systemSchema(model->schema);
      if (model->user) {
	userRelationCount++;
      }
      relationMap[model->schema].push_back(model);
    }

    bool unfoldSchemas = userRelationCount < 50;

    if (unfoldSchemas) {
      for (map<wxString, vector<RelationModel*> >::iterator iter = relationMap.begin(); iter != relationMap.end(); iter++) {
	wxString schema = iter->first;
	vector<RelationModel*> schemaRelations = iter->second;
	if (systemSchema(schema)) {
	  continue;
	}
	if (schemaRelations.size() == 1 && schemaRelations[0]->name.IsSameAs(_T(""))) {
	  continue;
	}
	ob->AddRelationItems(databaseItemId, schemaRelations, true);
      }
      for (map<wxString, vector<RelationModel*> >::iterator iter = relationMap.begin(); iter != relationMap.end(); iter++) {
	wxString schema = iter->first;
	vector<RelationModel*> schemaRelations = iter->second;
	if (!systemSchema(schema) && emptySchema(schemaRelations)) {
	  ob->AddSchemaItem(systemSchema(schema) ? databaseItemId : databaseItemId, schema, schemaRelations);
	}
      }
      wxTreeItemId systemRelationsItem = ob->AppendItem(databaseItemId, _("System relations"));
      for (map<wxString, vector<RelationModel*> >::iterator iter = relationMap.begin(); iter != relationMap.end(); iter++) {
	wxString schema = iter->first;
	vector<RelationModel*> schemaRelations = iter->second;
	if (systemSchema(schema)) {
	  ob->AddSchemaItem(systemRelationsItem, schema, schemaRelations);
	}
      }
    }
    else {
      wxTreeItemId systemRelationsItem = ob->AppendItem(databaseItemId, _("System relations"));
      for (map<wxString, vector<RelationModel*> >::iterator iter = relationMap.begin(); iter != relationMap.end(); iter++) {
	wxString schema = iter->first;
	vector<RelationModel*> schemaRelations = iter->second;
	ob->AddSchemaItem(systemSchema(schema) ? systemRelationsItem : databaseItemId, schema, schemaRelations);
      }
    }

    wxLogDebug(_T("%p: finished loading database schema"), this);

    ob->LoadedDatabase(databaseItemId);
  }
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
  wxTreeItemId serverItem = AppendItem(GetRootItem(), connName);
  SetItemText(serverItem, connName);
  SetItemData(serverItem, serverModel);

  RefreshDatabaseList(serverItem);
}

void ObjectBrowser::dispose() {
  for (vector<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    (*iter)->conn->dispose();
  }
}

void ObjectBrowser::RefreshDatabaseList(wxTreeItemId serverItem) {
  ServerModel *serverModel = dynamic_cast<ServerModel*>(GetItemData(serverItem));
  DatabaseConnection *conn = serverModel->conn->getConnection();
  conn->AddWork(new RefreshDatabaseListWork(this, serverModel, serverItem));
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
    Delete(firstChildItem);
    // veto expand event, for lazy loader to complete
    event.Veto();
  }
}

void ObjectBrowser::LoadDatabase(wxTreeItemId databaseItemId, DatabaseModel *database) {
  const wxCharBuffer dbnameBuf = database->name.utf8_str();
  DatabaseConnection *conn = database->server->getConnection(dbnameBuf);
  conn->AddWork(new LoadDatabaseSchemaWork(this, database, databaseItemId));
}

void ObjectBrowser::AddDatabaseItem(wxTreeItemId parent, DatabaseModel *database) {
  wxTreeItemId databaseItem = AppendItem(parent, database->name);
  SetItemData(databaseItem, database);
  if (database->usable())
    SetItemData(AppendItem(databaseItem, _("Loading...")), new DatabaseLoader(this, database));
}

void ObjectBrowser::AddSchemaItem(wxTreeItemId parent, wxString schemaName, vector<RelationModel*> relations) {
  wxTreeItemId schemaItem = AppendItem(parent, schemaName);
  if (relations.size() == 1 && relations[0]->name.IsSameAs(_T(""))) {
    return;
  }
  AddRelationItems(schemaItem, relations, false);
}

void ObjectBrowser::AddRelationItems(wxTreeItemId parent, vector<RelationModel*> relations, bool qualify) {
  for (vector<RelationModel*>::iterator iter = relations.begin(); iter != relations.end(); iter++) {
    wxString name;
    if (qualify)
      name = (*iter)->schema + _T(".") + (*iter)->name;
    else
      name = (*iter)->name;
    AppendItem(parent, name);
  }
}
