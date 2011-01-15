// -*- c++ -*-

#ifndef __object_browser_h
#define __object_browser_h

#include "wx/treectrl.h"
#include "server_connection.h"
#include "database_connection.h"

#include <string>
#include <vector>

using namespace std;

class DatabaseModel : public wxTreeItemData {
public:
  DatabaseModel() {
    conn = NULL;
  }
  DatabaseConnection *conn;
  ServerConnection *server;
  int oid;
  int isTemplate : 1;
  int allowConnections : 1;
  int havePrivsToConnect : 1;
  wxString name;
};

class UserModel : public wxTreeItemData {
public:
  int oid;
  int isSuperuser;
  wxString name;
};

class GroupModel : public wxTreeItemData {
public:
  int oid;
  wxString name;
};

class ServerModel : public wxTreeItemData {
public:
  ServerConnection *conn;
  vector<DatabaseModel*> databases;
  DatabaseModel *findDatabase(int oid) {
    for (vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
      if ((*iter)->oid == oid)
	return *iter;
    }
    return NULL;
  }
};

class TablespaceModel : public wxTreeItemData {
public:
  int oid;
  wxString name;
};

class ObjectBrowser : public wxTreeCtrl {
public:
  ObjectBrowser(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTR_HAS_BUTTONS|wxTR_HIDE_ROOT);
  ~ObjectBrowser() {
    dispose();
  }

  void AddServerConnection(ServerConnection *conn);
  void LoadDatabase(DatabaseModel *);

  void dispose();

private:
  DECLARE_EVENT_TABLE();
  vector<ServerModel*> servers;
  void RefreshDatabaseList(wxTreeItemId serverItem);
  void BeforeExpand(wxTreeEvent&);
};

class LazyLoader : public wxTreeItemData {
public:
  virtual void load() = 0;
};

class DatabaseLoader : public LazyLoader {
public:
  DatabaseLoader(ObjectBrowser *objectBrowser_, DatabaseModel *db_) {
    objectBrowser = objectBrowser_;
    db = db_;
  }

  void load() {
    objectBrowser->LoadDatabase(db);
  }
  
private:
  DatabaseModel *db;
  ObjectBrowser *objectBrowser;
};

#endif
