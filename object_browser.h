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
  void AddDatabaseItem(wxTreeItemId parent, DatabaseModel *database);
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
