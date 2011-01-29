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

BEGIN_EVENT_TABLE(ObjectBrowser, wxTreeCtrl)
  EVT_TREE_ITEM_EXPANDING(Pqwx_ObjectBrowser, ObjectBrowser::BeforeExpand)
END_EVENT_TABLE()

class InvokeEventOnCompletion : DatabaseWorkCompletionPort {
public:
  InvokeEventOnCompletion(wxEvtHandler *dest) : dest(dest) { }
  virtual void complete(bool result) {
    dest->AddPendingEvent(event(result));
  }
  virtual wxEvent& event(bool result) = 0;
private:
  wxEvtHandler *dest;
};

class InvokeStaticEventOnCompletion : InvokeEventOnCompletion {
public:
  InvokeStaticEventOnCompletion(wxEvtHandler *dest, wxEvent& event) : InvokeEventOnCompletion(dest), theEvent(&event) { }
  virtual wxEvent& event(bool result) {
    return *theEvent;
  }
private:
  wxEvent *theEvent;
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

  wxTreeItemId serverItem = AppendItem(GetRootItem(), nameOf(conn));
  SetItemData(serverItem, serverModel);

  RefreshDatabaseList(serverItem);
}

void ObjectBrowser::dispose() {
  for (vector<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    (*iter)->conn->dispose();
  }
}

#define GET_OID(iter, index, result) (*iter)[index].ToULong(&(result))
#define GET_BOOLEAN(iter, index, result) result = (*iter)[index].IsSameAs(_T("t"))
#define GET_TEXT(iter, index, result) result = (*iter)[index]

void ObjectBrowser::RefreshDatabaseList(wxTreeItemId serverItem) {
  ServerModel *serverModel = dynamic_cast<ServerModel*>(GetItemData(serverItem));
  DatabaseConnection *conn = serverModel->conn->getConnection();

  QueryResults databases;
  conn->ExecQuery("SELECT oid, datname, datistemplate, datallowconn, has_database_privilege(oid, 'connect') AS can_connect FROM pg_database", databases);

  vector<DatabaseModel*> userDatabases;
  vector<DatabaseModel*> templateDatabases;
  vector<DatabaseModel*> systemDatabases;
  vector<TablespaceModel*> userTablespaces;
  vector<TablespaceModel*> systemTablespaces;

  for (QueryResults::iterator iter = databases.begin(); iter != databases.end(); iter++) {
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

  QueryResults tablespaces;
  conn->ExecQuery("SELECT oid, spcname FROM pg_tablespace", tablespaces);
  for (QueryResults::iterator iter = tablespaces.begin(); iter != tablespaces.end(); iter++) {
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
    AddDatabaseItem(serverItem, *iter);
  }

  for (vector<TablespaceModel*>::iterator iter = userTablespaces.begin(); iter != userTablespaces.end(); iter++) {
    wxTreeItemId tablespaceItem = AppendItem(serverItem, (*iter)->name);
    SetItemData(tablespaceItem, *iter);
  }

  wxTreeItemId usersItem = AppendItem(serverItem, _("Users"));
  wxTreeItemId groupsItem = AppendItem(serverItem, _("Groups"));
  wxTreeItemId sysDatabasesItem = AppendItem(serverItem, _("System databases"));
  wxTreeItemId sysTablespacesItem = AppendItem(serverItem, _("System tablespaces"));

  QueryResults roles;
  conn->ExecQuery("SELECT oid, rolname, rolcanlogin, rolsuper FROM pg_roles", roles);
  for (QueryResults::iterator iter = roles.begin(); iter != roles.end(); iter++) {
    int canLogin;
    GET_BOOLEAN(iter, 2, canLogin);
    if (canLogin) {
      UserModel *userModel = new UserModel();
      GET_OID(iter, 0, userModel->oid);
      GET_TEXT(iter, 1, userModel->name);
      GET_BOOLEAN(iter, 3, userModel->isSuperuser);
      wxTreeItemId userItemId = AppendItem(usersItem, userModel->name);
    }
    else {
      GroupModel *groupModel = new GroupModel();
      GET_OID(iter, 0, groupModel->oid);
      GET_TEXT(iter, 1, groupModel->name);
      wxTreeItemId groupItemId = AppendItem(groupsItem, groupModel->name);
    }
  }

  for (vector<DatabaseModel*>::iterator iter = systemDatabases.begin(); iter != systemDatabases.end(); iter++) {
    AddDatabaseItem(sysDatabasesItem, *iter);
  }

  for (vector<TablespaceModel*>::iterator iter = systemTablespaces.begin(); iter != systemTablespaces.end(); iter++) {
    wxTreeItemId tablespaceItem = AppendItem(sysTablespacesItem, (*iter)->name);
    SetItemData(tablespaceItem, *iter);
  }

  if (servers.size() == 1) {
    Expand(serverItem);
  }
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
    // veto expand event if we did not produce any replacement items
    if (GetChildrenCount(expandingItem) == 0) {
      event.Veto();
    }
  }
}

static inline bool systemSchema(wxString schema) {
  return schema.StartsWith(_T("pg_")) || schema.IsSameAs(_T("information_schema"));
}

static inline bool emptySchema(vector<RelationModel*> schemaRelations) {
  return schemaRelations.size() == 1 && schemaRelations[0]->name.IsSameAs(_T(""));
}

void ObjectBrowser::LoadDatabase(wxTreeItemId databaseItemId, DatabaseModel *database) {
  const wxCharBuffer dbnameBuf = database->name.utf8_str();
  DatabaseConnection *conn = database->server->getConnection(dbnameBuf);

  QueryResults relations;
  conn->ExecQuery("SELECT pg_class.oid, nspname, relname, relkind FROM (SELECT oid, relname, relkind, relnamespace FROM pg_class WHERE relkind IN ('r','v') OR (relkind = 'S' AND NOT EXISTS (SELECT 1 FROM pg_depend WHERE classid = 'pg_class'::regclass AND objid = pg_class.oid AND refclassid = 'pg_class'::regclass AND deptype = 'a'))) pg_class RIGHT JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace", relations);

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
      AddRelationItems(databaseItemId, schemaRelations, true);
    }
    for (map<wxString, vector<RelationModel*> >::iterator iter = relationMap.begin(); iter != relationMap.end(); iter++) {
      wxString schema = iter->first;
      vector<RelationModel*> schemaRelations = iter->second;
      if (!systemSchema(schema) && emptySchema(schemaRelations)) {
	AddSchemaItem(systemSchema(schema) ? databaseItemId : databaseItemId, schema, schemaRelations);
      }
    }
    wxTreeItemId systemRelationsItem = AppendItem(databaseItemId, _("System relations"));
    for (map<wxString, vector<RelationModel*> >::iterator iter = relationMap.begin(); iter != relationMap.end(); iter++) {
      wxString schema = iter->first;
      vector<RelationModel*> schemaRelations = iter->second;
      if (systemSchema(schema)) {
	AddSchemaItem(systemRelationsItem, schema, schemaRelations);
      }
    }
  }
  else {
    wxTreeItemId systemRelationsItem = AppendItem(databaseItemId, _("System relations"));
    for (map<wxString, vector<RelationModel*> >::iterator iter = relationMap.begin(); iter != relationMap.end(); iter++) {
      wxString schema = iter->first;
      vector<RelationModel*> schemaRelations = iter->second;
      AddSchemaItem(systemSchema(schema) ? systemRelationsItem : databaseItemId, schema, schemaRelations);
    }
  }
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
