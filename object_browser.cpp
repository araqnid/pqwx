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

ObjectBrowser::ObjectBrowser(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style) : wxTreeCtrl(parent, id, pos, size, style) {
  AddRoot(_("root"));
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

void ObjectBrowser::RefreshDatabaseList(wxTreeItemId serverItem) {
  ServerModel *serverModel = dynamic_cast<ServerModel*>(GetItemData(serverItem));

  vector<DatabaseInfo> databaseList;
  serverModel->conn->getConnection()->listDatabases(databaseList);

  vector<RoleInfo> roleList;
  serverModel->conn->getConnection()->listRoles(roleList);

  vector<TablespaceInfo> tablespaceList;
  serverModel->conn->getConnection()->listTablespaces(tablespaceList);

  vector<DatabaseModel*> userDatabases;
  vector<DatabaseModel*> templateDatabases;
  vector<DatabaseModel*> systemDatabases;
  vector<TablespaceModel*> userTablespaces;
  vector<TablespaceModel*> systemTablespaces;

  for (vector<DatabaseInfo>::iterator iter = databaseList.begin(); iter != databaseList.end(); iter++) {
    DatabaseModel *databaseModel = new DatabaseModel();
    databaseModel->server = serverModel->conn;
    databaseModel->oid = iter->oid;
    databaseModel->isTemplate = iter->isTemplate;
    databaseModel->allowConnections = iter->allowConnections;
    databaseModel->havePrivsToConnect = iter->havePrivsToConnect;
    databaseModel->name = wxString(iter->name.c_str(), wxConvUTF8);
    serverModel->databases.push_back(databaseModel);
    if (iter->name.compare("postgres") == 0 || iter->name.compare("template0") == 0 || iter->name.compare("template1") == 0) {
      systemDatabases.push_back(databaseModel);
    }
    else if (iter->isTemplate) {
      templateDatabases.push_back(databaseModel);
    }
    else {
      userDatabases.push_back(databaseModel);
    }
  }

  for (vector<TablespaceInfo>::iterator iter = tablespaceList.begin(); iter != tablespaceList.end(); iter++) {
    TablespaceModel *tablespaceModel = new TablespaceModel();
    tablespaceModel->oid = iter->oid;
    tablespaceModel->name = wxString(iter->name.c_str(), wxConvUTF8);
    if (iter->name.substr(0, 3).compare("pg_") == 0) {
      systemTablespaces.push_back(tablespaceModel);
    }
    else {
      userTablespaces.push_back(tablespaceModel);
    }
  }

  for (vector<DatabaseModel*>::iterator iter = userDatabases.begin(); iter != userDatabases.end(); iter++) {
    wxTreeItemId databaseItem = AppendItem(serverItem, (*iter)->name);
    SetItemData(databaseItem, *iter);
    SetItemData(AppendItem(databaseItem, _("Loading...")), new DatabaseLoader(this, *iter));
  }

  for (vector<TablespaceModel*>::iterator iter = userTablespaces.begin(); iter != userTablespaces.end(); iter++) {
    wxTreeItemId tablespaceItem = AppendItem(serverItem, (*iter)->name);
    SetItemData(tablespaceItem, *iter);
  }

  wxTreeItemId usersItem = AppendItem(serverItem, _("Users"));
  wxTreeItemId groupsItem = AppendItem(serverItem, _("Groups"));
  wxTreeItemId sysDatabasesItem = AppendItem(serverItem, _("System databases"));
  wxTreeItemId sysTablespacesItem = AppendItem(serverItem, _("System tablespaces"));

  for (vector<RoleInfo>::iterator iter = roleList.begin(); iter != roleList.end(); iter++) {
    if (iter->canLogin) {
      UserModel *userModel = new UserModel();
      userModel->oid = iter->oid;
      userModel->isSuperuser = iter->isSuperuser;
      userModel->name = wxString(iter->name.c_str(), wxConvUTF8);
      wxTreeItemId userItemId = AppendItem(usersItem, userModel->name);
    }
    else {
      GroupModel *groupModel = new GroupModel();
      groupModel->oid = iter->oid;
      groupModel->name = wxString(iter->name.c_str(), wxConvUTF8);
      wxTreeItemId groupItemId = AppendItem(groupsItem, groupModel->name);
    }
  }

  for (vector<DatabaseModel*>::iterator iter = systemDatabases.begin(); iter != systemDatabases.end(); iter++) {
    wxTreeItemId databaseItem = AppendItem(sysDatabasesItem, (*iter)->name);
    SetItemData(databaseItem, *iter);
    SetItemData(AppendItem(databaseItem, _("Loading...")), new DatabaseLoader(this, *iter));
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
    lazyLoader->load();
    Delete(firstChildItem);
    // veto expand event if we did not produce any replacement items
    if (GetChildrenCount(expandingItem) == 0) {
      event.Veto();
    }
  }
}

void ObjectBrowser::LoadDatabase(DatabaseModel *database) {
  const wxCharBuffer dbnameBuf = database->name.utf8_str();
  DatabaseConnection *conn = database->server->getConnection(dbnameBuf);

  vector<RelationInfo> relationList;
  conn->listRelations(relationList);
  for (vector<RelationInfo>::iterator iter = relationList.begin(); iter != relationList.end(); iter++) {
  }
}
