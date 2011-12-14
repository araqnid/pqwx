// -*- c++ -*-

#ifndef __object_browser_h
#define __object_browser_h

#include "wx/treectrl.h"
#include "server_connection.h"
#include "database_connection.h"

#include <string>
#include <vector>

using namespace std;

class RelationModel : public wxTreeItemData {
public:
  unsigned long oid;
  wxString schema;
  wxString name;
  int user : 1;
  enum { TABLE, VIEW, SEQUENCE } type;
};

class DatabaseModel : public wxTreeItemData {
public:
  DatabaseModel() {
    conn = NULL;
  }
  DatabaseConnection *conn;
  ServerConnection *server;
  unsigned long oid;
  int isTemplate : 1;
  int allowConnections : 1;
  int havePrivsToConnect : 1;
  wxString name;
  int usable() {
    return allowConnections && havePrivsToConnect;
  }
};

class UserModel : public wxTreeItemData {
public:
  unsigned long oid;
  int isSuperuser;
  wxString name;
};

class GroupModel : public wxTreeItemData {
public:
  unsigned long oid;
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

class TablespaceModel : public wxTreeItemData {
public:
  unsigned long oid;
  wxString name;
};

class ObjectBrowser : public wxTreeCtrl {
public:
  ObjectBrowser(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTR_HAS_BUTTONS|wxTR_HIDE_ROOT);
  ~ObjectBrowser() {
    dispose();
  }

  void AddServerConnection(ServerConnection *conn);
  void LoadDatabase(wxTreeItemId parent, DatabaseModel *);

  void dispose();

private:
  DECLARE_EVENT_TABLE();
  vector<ServerModel*> servers;
  void RefreshDatabaseList(wxTreeItemId serverItem);
  void BeforeExpand(wxTreeEvent&);
  void OnWorkFinished(wxCommandEvent&);
  void SubmitServerWork(ServerModel *server, DatabaseWork *work);
  void SubmitDatabaseWork(DatabaseModel *database, DatabaseWork *work);
public: // bodge to make these accessible... more refactoring needed
  void AddDatabaseItem(wxTreeItemId parent, DatabaseModel *database);
  void AddSchemaItem(wxTreeItemId parent, wxString schemaName, vector<RelationModel*> relations);
  void AddRelationItems(wxTreeItemId parent, vector<RelationModel*> relations, bool qualify);
  void AddTablespaceItem(wxTreeItemId parent, TablespaceModel *tablespace) {
      wxTreeItemId tablespaceItem = AppendItem(parent, tablespace->name);
      SetItemData(tablespaceItem, tablespace);
  }
  void AddSystemItems(wxTreeItemId serverItem) {
    wxTreeItemId usersItem = AppendItem(serverItem, _("Users"));
    wxTreeItemId groupsItem = AppendItem(serverItem, _("Groups"));
    wxTreeItemId sysDatabasesItem = AppendItem(serverItem, _("System databases"));
    wxTreeItemId sysTablespacesItem = AppendItem(serverItem, _("System tablespaces"));
  }
  void AddUserItem(wxTreeItemId serverItem, UserModel *userModel) {
    //wxTreeItemId userItemId = AppendItem(usersItem, userModel->name);
  }
  void AddGroupItem(wxTreeItemId serverItem, GroupModel *groupModel) {
    //wxTreeItemId groupItemId = AppendItem(groupsItem, groupModel->name);
  }
  void AddSystemDatabaseItem(wxTreeItemId serverItem, DatabaseModel *databaseModel) {
    AddDatabaseItem(serverItem, databaseModel); // should add to sysDatabasesItem
  }
  void AddSystemTablespaceItem(wxTreeItemId serverItem, TablespaceModel *tablespaceModel) {
    wxTreeItemId tablespaceItem = AppendItem(serverItem, tablespaceModel->name); // should add to sysTablespacesItem
    SetItemData(tablespaceItem, tablespaceModel);
  }
  void LoadedServer(wxTreeItemId serverItem) {
    if (servers.size() == 1) {
      Expand(serverItem);
    }
  }
  void LoadedDatabase(wxTreeItemId databaseItem, DatabaseModel *database) {
    Expand(databaseItem);
    SetItemText(databaseItem, database->name);
  }
};

class LazyLoader : public wxTreeItemData {
public:
  virtual void load(wxTreeItemId parent) = 0;
};

class DatabaseLoader : public LazyLoader {
public:
  DatabaseLoader(ObjectBrowser *objectBrowser_, DatabaseModel *db_) {
    objectBrowser = objectBrowser_;
    db = db_;
  }

  void load(wxTreeItemId parent) {
    objectBrowser->LoadDatabase(parent, db);
  }
  
private:
  DatabaseModel *db;
  ObjectBrowser *objectBrowser;
};

#endif
