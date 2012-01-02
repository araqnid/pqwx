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
#include "database_event_type.h"

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

BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(PQWX_ObjectSelected, -1)
END_DECLARE_EVENT_TYPES()

#define PQWX_OBJECT_SELECTED(id, fn) EVT_DATABASE(id, PQWX_ObjectSelected, fn)

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

  void AddServerConnection(const ServerConnection& server, DatabaseConnection *db);
  void LoadDatabase(wxTreeItemId parent, DatabaseModel *db, IndexSchemaCompletionCallback *indexCompletion = NULL);
  void LoadRelation(wxTreeItemId parent, RelationModel *rel);
  void DisconnectSelected();
  void FindObject(const ServerConnection &server, const wxString &dbname);
  void FindObject(const DatabaseModel*);
  void ZoomToFoundObject(const DatabaseModel *database, const CatalogueIndex::Document *document) { ZoomToFoundObject(database, document->entityId); }
  void ZoomToFoundObject(const DatabaseModel *database, Oid entityId);

  void Dispose();

  void SubmitServerWork(ServerModel *server, ObjectBrowserWork *work);
  void SubmitDatabaseWork(DatabaseModel *database, ObjectBrowserWork *work);
  void ConnectAndAddWork(DatabaseConnection *db, ObjectBrowserWork *work);

  void FillInServer(ServerModel *serverModel, wxTreeItemId serverItem);
  void FillInDatabaseSchema(DatabaseModel *database, wxTreeItemId databaseItem);

  void FillInRelation(RelationModel *relation, wxTreeItemId relationItem, std::vector<ColumnModel*> &columns, std::vector<IndexModel*> &indices, std::vector<TriggerModel*> &triggers);

  void AppendDatabaseItems(wxTreeItemId parent, std::vector<DatabaseModel*> &database);
  void AppendDivision(std::vector<SchemaMemberModel*> &members, wxTreeItemId parentItem);
  void DivideSchemaMembers(std::vector<SchemaMemberModel*> &members, std::vector<SchemaMemberModel*> &userDivision, std::vector<SchemaMemberModel*> &systemDivision, std::map<wxString, std::vector<SchemaMemberModel*> > &extensionDivisions);
  void AppendSchemaMembers(wxTreeItemId parent, bool createSchemaItem, const wxString &schemaName, const std::vector<SchemaMemberModel*> &members);

  ServerModel *FindServer(const ServerConnection &server) const;
  DatabaseModel *FindDatabase(const ServerConnection &server, const wxString &dbname) const;

  wxTreeItemId FindServerItem(const ServerModel *server) const;
  wxTreeItemId FindDatabaseItem(const DatabaseModel *db) const;
  wxTreeItemId FindSystemSchemasItem(const DatabaseModel *db) const;
  LazyLoader* GetLazyLoader(wxTreeItemId item) const;
  void DeleteLazyLoader(wxTreeItemId item);

  void UnmarkSelected() { currentlySelected = false; }

  static const VersionedSql& GetSqlDictionary();
private:
  DECLARE_EVENT_TABLE();
  std::list<ServerModel*> servers;
  void RefreshDatabaseList(wxTreeItemId serverItem);
  void BeforeExpand(wxTreeEvent&);
  void OnItemSelected(wxTreeEvent&);
  void OnSetFocus(wxFocusEvent&);
  void OnGetTooltip(wxTreeEvent&);
  void OnItemRightClick(wxTreeEvent&);
  void OnWorkFinished(wxCommandEvent&);
  void OnServerMenuDisconnect(wxCommandEvent&);
  void OnServerMenuRefresh(wxCommandEvent&);
  void OnServerMenuProperties(wxCommandEvent&);
  void OnDatabaseMenuQuery(wxCommandEvent&);
  void OnDatabaseMenuRefresh(wxCommandEvent&);
  void OnDatabaseMenuProperties(wxCommandEvent&);
  void OnDatabaseMenuViewDependencies(wxCommandEvent&);
  void OnRelationMenuViewDependencies(wxCommandEvent&);
  void OnFunctionMenuViewDependencies(wxCommandEvent&);

  void FillInDatabases(ServerModel *serverModel, wxTreeItemId serverItem);
  void FillInRoles(ServerModel *serverModel, wxTreeItemId serverItem);

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

  void FindItemContext(const wxTreeItemId &item, ServerModel **server, DatabaseModel **database) const;

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

  // remember which server/database was last mentioned in an object-selected event
  ServerModel *selectedServer;
  DatabaseModel *selectedDatabase;
  bool currentlySelected;
  void UpdateSelectedDatabase();

  // see constructor implementation for the image list initialisation these correspond to
  static const int img_folder = 0;
  static const int img_server = 1;
  static const int img_database = 2;
  static const int img_table = 3;
  static const int img_view = img_table + 1;
  static const int img_sequence = img_view + 1;
  static const int img_function = img_sequence + 1;
  static const int img_function_aggregate = img_function + 1;
  static const int img_function_trigger = img_function_aggregate + 1;
  static const int img_function_window = img_function_trigger + 1;
};

const int EVENT_WORK_FINISHED = 10000;

#endif
