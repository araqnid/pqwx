// -*- c++ -*-

#ifndef __object_browser_h
#define __object_browser_h

#include "wx/treectrl.h"
#include "server_connection.h"
#include "database_connection.h"

#include <string>
#include <vector>

using namespace std;

class ServerModel;
class DatabaseModel;
class RoleModel;
class RelationModel;
class ColumnModel;

class ObjectBrowser : public wxTreeCtrl {
public:
  ObjectBrowser(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTR_HAS_BUTTONS|wxTR_HIDE_ROOT);
  ~ObjectBrowser() {
    dispose();
  }

  void AddServerConnection(ServerConnection *conn);
  void LoadDatabase(wxTreeItemId parent, DatabaseModel *);
  void LoadRelation(wxTreeItemId parent, RelationModel *);

  void dispose();

  void SubmitServerWork(ServerModel *server, DatabaseWork *work);
  void SubmitDatabaseWork(DatabaseModel *database, DatabaseWork *work);

  void FillInServer(ServerModel *serverModel, wxTreeItemId serverItem, wxString& serverVersion);
  void FillInDatabases(ServerModel *serverModel, wxTreeItemId serverItem, vector<DatabaseModel*> &databases);
  void FillInRoles(ServerModel *serverModel, wxTreeItemId serverItem, vector<RoleModel*> &roles);

  void FillInDatabaseSchema(DatabaseModel *database, wxTreeItemId databaseItem, vector<RelationModel*> &relations);

  void FillInRelation(RelationModel *relation, wxTreeItemId relationItem, vector<ColumnModel*> &columns);
private:
  DECLARE_EVENT_TABLE();
  vector<ServerModel*> servers;
  void RefreshDatabaseList(wxTreeItemId serverItem);
  void BeforeExpand(wxTreeEvent&);
  void OnWorkFinished(wxCommandEvent&);
  void AppendDatabaseItems(wxTreeItemId parent, vector<DatabaseModel*> &database);
  void AppendSchemaItems(wxTreeItemId parent, bool includeSchemaItem, const wxString &schemaName, vector<RelationModel*> &relations);
};

#endif
