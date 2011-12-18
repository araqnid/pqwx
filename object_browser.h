// -*- c++ -*-

#ifndef __object_browser_h
#define __object_browser_h

#include <vector>
#include <list>
#include "wx/treectrl.h"
#include "server_connection.h"
#include "database_connection.h"
#include "versioned_sql.h"
#include "catalogue_index.h"

using namespace std;

class ServerModel;
class DatabaseModel;
class RoleModel;
class RelationModel;
class ColumnModel;
class IndexModel;
class TriggerModel;
class FunctionModel;
class SchemaMemberModel;

#define DECLARE_SCRIPT_HANDLERS(menu, mode) \
  void On##menu##MenuScript##mode##Window(wxCommandEvent&); \
  void On##menu##MenuScript##mode##File(wxCommandEvent&); \
  void On##menu##MenuScript##mode##Clipboard(wxCommandEvent&)

class ObjectBrowser : public wxTreeCtrl {
public:
  ObjectBrowser(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTR_HAS_BUTTONS|wxTR_HIDE_ROOT);

  ~ObjectBrowser() {
    Dispose();
    delete sql;
  }

  void AddServerConnection(ServerConnection *server, DatabaseConnection *db);
  void LoadDatabase(wxTreeItemId parent, DatabaseModel *);
  void LoadRelation(wxTreeItemId parent, RelationModel *);
  void DisconnectSelected();
  void FindObject();
  void ZoomToFoundObject(DatabaseModel*, const CatalogueIndex::Document*);

  void Dispose();

  DatabaseConnection* GetServerAdminConnection(ServerModel *server);
  DatabaseConnection* GetDatabaseConnection(ServerModel *server, const wxString &dbname);
  void SubmitServerWork(ServerModel *server, DatabaseWork *work);
  void SubmitDatabaseWork(DatabaseModel *database, DatabaseWork *work);
  void ConnectAndAddWork(ServerModel *server, DatabaseConnection *db, DatabaseWork *work);

  void FillInServer(ServerModel *serverModel, wxTreeItemId serverItem, const wxString& serverVersionString, int serverVersion, bool usingSSL);
  void FillInDatabases(ServerModel *serverModel, wxTreeItemId serverItem, vector<DatabaseModel*> &databases);
  void FillInRoles(ServerModel *serverModel, wxTreeItemId serverItem, vector<RoleModel*> &roles);

  void FillInDatabaseSchema(DatabaseModel *database, wxTreeItemId databaseItem, vector<RelationModel*> &relations, vector<FunctionModel*> &functions);

  void FillInRelation(RelationModel *relation, wxTreeItemId relationItem, vector<ColumnModel*> &columns, vector<IndexModel*> &indices, vector<TriggerModel*> &triggers);

  const char *GetSql(const wxString &name, int serverVersion) { return sql->GetSql(name, serverVersion); }
private:
  DECLARE_EVENT_TABLE();
  list<ServerModel*> servers;
  void RefreshDatabaseList(wxTreeItemId serverItem);
  void BeforeExpand(wxTreeEvent&);
  void OnGetTooltip(wxTreeEvent&);
  void OnItemRightClick(wxTreeEvent&);
  void OnWorkFinished(wxCommandEvent&);
  void OnServerMenuDisconnect(wxCommandEvent&);
  void OnServerMenuRefresh(wxCommandEvent&);
  void OnServerMenuProperties(wxCommandEvent&);
  void OnDatabaseMenuRefresh(wxCommandEvent&);
  void OnDatabaseMenuProperties(wxCommandEvent&);
  DECLARE_SCRIPT_HANDLERS(Database, Create);
  DECLARE_SCRIPT_HANDLERS(Database, Alter);
  DECLARE_SCRIPT_HANDLERS(Database, Drop);
  void AppendDatabaseItems(wxTreeItemId parent, vector<DatabaseModel*> &database);
  void AppendSchemaMembers(wxTreeItemId parent, bool includeSchemaMember, const wxString &schemaName, vector<SchemaMemberModel*> &members);
  VersionedSql *sql;

  // context menus
  wxMenu *serverMenu;
  wxMenu *databaseMenu;

  // remember what was the context for a context menu
  ServerModel *contextMenuServer;
  DatabaseModel *contextMenuDatabase;
  wxTreeItemId contextMenuItem;
};

const int EVENT_WORK_FINISHED = 10000;

#endif
