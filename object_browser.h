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
#include "lazy_loader.h"

class ServerModel;
class DatabaseModel;
class RoleModel;
class RelationModel;
class ColumnModel;
class IndexModel;
class TriggerModel;
class FunctionModel;
class SchemaMemberModel;
class ObjectBrowserWork;

#define DECLARE_SCRIPT_HANDLERS(menu, mode) \
  void On##menu##MenuScript##mode##Window(wxCommandEvent&); \
  void On##menu##MenuScript##mode##File(wxCommandEvent&); \
  void On##menu##MenuScript##mode##Clipboard(wxCommandEvent&)

class IndexSchemaCompletionCallback;

class ObjectBrowser : public wxTreeCtrl {
public:
  ObjectBrowser(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTR_HAS_BUTTONS|wxTR_HIDE_ROOT);

  ~ObjectBrowser() {
    Dispose();
  }

  void AddServerConnection(ServerConnection *server, DatabaseConnection *db);
  void LoadDatabase(wxTreeItemId parent, DatabaseModel *db, IndexSchemaCompletionCallback *indexCompletion = NULL);
  void LoadRelation(wxTreeItemId parent, RelationModel *rel);
  void DisconnectSelected();
  void FindObject();
  void ZoomToFoundObject(DatabaseModel*, const CatalogueIndex::Document*);

  void Dispose();

  void SubmitServerWork(ServerModel *server, ObjectBrowserWork *work);
  void SubmitDatabaseWork(DatabaseModel *database, ObjectBrowserWork *work);
  void ConnectAndAddWork(ServerModel *server, DatabaseConnection *db, ObjectBrowserWork *work);

  void FillInServer(ServerModel *serverModel, wxTreeItemId serverItem);
  void FillInDatabases(ServerModel *serverModel, wxTreeItemId serverItem, std::vector<DatabaseModel*> &databases);
  void FillInRoles(ServerModel *serverModel, wxTreeItemId serverItem, std::vector<RoleModel*> &roles);

  void FillInDatabaseSchema(DatabaseModel *database, wxTreeItemId databaseItem);

  void FillInRelation(RelationModel *relation, wxTreeItemId relationItem, std::vector<ColumnModel*> &columns, std::vector<IndexModel*> &indices, std::vector<TriggerModel*> &triggers);

  void AppendDatabaseItems(wxTreeItemId parent, std::vector<DatabaseModel*> &database);
  void AppendDivision(std::vector<SchemaMemberModel*> &members, wxTreeItemId parentItem);
  void DivideSchemaMembers(std::vector<SchemaMemberModel*> &members, std::vector<SchemaMemberModel*> &userDivision, std::vector<SchemaMemberModel*> &systemDivision, std::map<wxString, std::vector<SchemaMemberModel*> > &extensionDivisions);
  void AppendSchemaMembers(wxTreeItemId parent, bool createSchemaItem, const wxString &schemaName, const std::vector<SchemaMemberModel*> &members);

  wxTreeItemId FindServerItem(ServerModel *server) const;
  wxTreeItemId FindDatabaseItem(DatabaseModel *db) const;
  wxTreeItemId FindSystemSchemasItem(DatabaseModel *db) const;
  LazyLoader* GetLazyLoader(wxTreeItemId item) const;
  void DeleteLazyLoader(wxTreeItemId item);

  static const VersionedSql& GetSqlDictionary();
private:
  DECLARE_EVENT_TABLE();
  std::list<ServerModel*> servers;
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
  void OnDatabaseMenuViewDependencies(wxCommandEvent&);
  void OnRelationMenuViewDependencies(wxCommandEvent&);
  void OnFunctionMenuViewDependencies(wxCommandEvent&);
  DECLARE_SCRIPT_HANDLERS(Database, Create);
  DECLARE_SCRIPT_HANDLERS(Database, Alter);
  DECLARE_SCRIPT_HANDLERS(Database, Drop);
  DECLARE_SCRIPT_HANDLERS(Table, Create);
  DECLARE_SCRIPT_HANDLERS(Table, Drop);
  DECLARE_SCRIPT_HANDLERS(Table, Select);
  DECLARE_SCRIPT_HANDLERS(Table, Insert);
  DECLARE_SCRIPT_HANDLERS(Table, Update);
  DECLARE_SCRIPT_HANDLERS(Table, Delete);
  DECLARE_SCRIPT_HANDLERS(View, Create);
  DECLARE_SCRIPT_HANDLERS(View, Alter);
  DECLARE_SCRIPT_HANDLERS(View, Drop);
  DECLARE_SCRIPT_HANDLERS(View, Select);
  DECLARE_SCRIPT_HANDLERS(Sequence, Create);
  DECLARE_SCRIPT_HANDLERS(Sequence, Alter);
  DECLARE_SCRIPT_HANDLERS(Sequence, Drop);
  DECLARE_SCRIPT_HANDLERS(Function, Create);
  DECLARE_SCRIPT_HANDLERS(Function, Alter);
  DECLARE_SCRIPT_HANDLERS(Function, Drop);
  DECLARE_SCRIPT_HANDLERS(Function, Select);

  // context menus
  wxMenu *serverMenu;
  wxMenu *databaseMenu;
  wxMenu *tableMenu;
  wxMenu *viewMenu;
  wxMenu *sequenceMenu;
  wxMenu *functionMenu;

  // remember what was the context for a context menu
  ServerModel *contextMenuServer;
  DatabaseModel *contextMenuDatabase;
  RelationModel *contextMenuRelation;
  FunctionModel *contextMenuFunction;
  wxTreeItemId contextMenuItem;
};

const int EVENT_WORK_FINISHED = 10000;

#endif
