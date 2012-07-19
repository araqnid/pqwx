/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_h
#define __object_browser_h

#include <vector>
#include <list>
#include "wx/treectrl.h"
#include "wx/timer.h"
#include "server_connection.h"
#include "database_connection.h"
#include "sql_dictionary.h"
#include "catalogue_index.h"
#include "lazy_loader.h"
#include "database_event_type.h"
#include "object_browser_model.h"
#include "work_launcher.h"

BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(PQWX_ObjectSelected, -1)
  DECLARE_EVENT_TYPE(PQWX_NoObjectSelected, -1)
END_DECLARE_EVENT_TYPES()

/**
 * Event issued when an object is selected in the object browser.
 */
#define PQWX_OBJECT_SELECTED(id, fn) EVT_DATABASE(id, PQWX_ObjectSelected, fn)

/**
 * Event issued when no object is selected in the object browser.
 */
#define PQWX_NO_OBJECT_SELECTED(id, fn) EVT_COMMAND(id, PQWX_NoObjectSelected, fn)

/**
 * Declare handler methods for context menu scripting items.
 */
#define DECLARE_SCRIPT_HANDLERS(menu, mode) \
  void On##menu##MenuScript##mode##Window(wxCommandEvent&); \
  void On##menu##MenuScript##mode##File(wxCommandEvent&); \
  void On##menu##MenuScript##mode##Clipboard(wxCommandEvent&)

/**
 * The object browser tree control.
 */
class ObjectBrowser : public wxTreeCtrl {
public:
  /**
   * Create object browser.
   */
  ObjectBrowser(ObjectBrowserModel& model, wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTR_HAS_BUTTONS|wxTR_HIDE_ROOT);

  ~ObjectBrowser()
  {
    Dispose();
  }

  /**
   * Register a new server connection with the object browser.
   */
  void AddServerConnection(const ServerConnection& server, DatabaseConnection *db);
  /**
   * Load the detail for a specified relation.
   */
  void LoadRelation(const ObjectModelReference& relationRef);
  /**
   * Test if some server is selected, so disconnecting is valid.
   */
  bool IsServerSelected() const;
  /**
   * Disconnect from the currently-selected object.
   */
  void DisconnectSelected();
  /**
   * Open the object finder using the given server/database details.
   */
  void FindObject(const ServerConnection &server, const wxString &dbname);
  /**
   * Open the object finder for the specified database.
   */
  void FindObject(const ObjectModelReference& databaseRef);
  /**
   * Zoom to a particular object as a result of the object finder.
   */
  void ZoomToFoundObject(const ObjectModelReference& databaseRef, const CatalogueIndex::Document *document) { ZoomToFoundObject(databaseRef, document->entityId); }
  /**
   * Zoom to a particular database object.
   */
  void ZoomToFoundObject(const ObjectModelReference& databaseRef, Oid entityId);

  /**
   * Dispose of object browser view and model.
   */
  void Dispose();

  /**
   * Update server details tree after loading from the database.
   */
  void UpdateServer(const wxString& serverId, bool expandAfter);
  /**
   * Fill in database schema after loading from the database.
   */
  void UpdateDatabase(const ObjectModelReference& databaseRef, bool expandAfter);
  /**
   * Fill in relation details after loading from the database.
   */
  void UpdateRelation(const ObjectModelReference& relationRef);

  /**
   * Find the tree item for a server.
   */
  wxTreeItemId FindServerItem(const wxString& serverId) const;
  /**
   * Find the tree item for a database.
   */
  wxTreeItemId FindDatabaseItem(const ObjectModelReference& databaseRef) const;
  /**
   * Find the tree item for a relation.
   */
  wxTreeItemId FindRelationItem(const ObjectModelReference& relationRef) const;
  /**
   * Find the tree item for a relation.
   */
  wxTreeItemId FindRelationItem(const ObjectModelReference& databaseRef, Oid oid) const;
  /**
   * Find the tree item for the "system schemas" label in a database.
   */
  wxTreeItemId FindSystemSchemasItem(const ObjectModelReference& databaseRef) const;
  /**
   * Get the lazy loader under a tree item, if any.
   */
  LazyLoader* GetLazyLoader(wxTreeItemId item) const;
  /**
   * Delete the lazy loader under a tree item, if any.
   */
  void DeleteLazyLoader(wxTreeItemId item);

  /**
   * @return True if some object is "currently selected".
   */
  bool IsObjectSelected() const { return !selectedRef.GetServerId().empty(); }
  /**
   * Consider no object to be "currently selected".
   */
  void UnmarkSelected() { selectedRef = ObjectModelReference(wxEmptyString); }
  /*
   * @return Reference to the "currently selected" object.
   */
  ObjectModelReference GetSelected() const { return selectedRef; }

  /**
   * @return Object browser model
   */
  ObjectBrowserModel& Model() const { return model; }
private:
  DECLARE_EVENT_TABLE();
  ObjectBrowserModel& model;
  void BeforeExpand(wxTreeEvent&);
  void OnItemSelected(wxTreeEvent&);
  void OnSetFocus(wxFocusEvent&);
  void OnGetTooltip(wxTreeEvent&);
  void OnItemRightClick(wxTreeEvent&);
  void OnServerMenuNewDatabase(wxCommandEvent&);
  void OnServerMenuDisconnect(wxCommandEvent&);
  void OnServerMenuRefresh(wxCommandEvent&);
  void OnServerMenuProperties(wxCommandEvent&);
  void OnDatabaseMenuQuery(wxCommandEvent&);
  void OnDatabaseMenuDrop(wxCommandEvent&);
  void OnDatabaseMenuRefresh(wxCommandEvent&);
  void OnDatabaseMenuProperties(wxCommandEvent&);
  void OnDatabaseMenuViewDependencies(wxCommandEvent&);
  void OnRelationMenuViewDependencies(wxCommandEvent&);
  void OnFunctionMenuViewDependencies(wxCommandEvent&);

  const ServerModel* ContextMenuServer();
  void EnableIffHaveCreateDBPrivilege(wxUpdateUIEvent &event);
  const DatabaseModel* ContextMenuDatabase();
  void EnableIffDroppableDatabase(wxUpdateUIEvent &event);
  void EnableIffUsableDatabase(wxUpdateUIEvent &event);
  void PrepareServerMenu(const ServerModel*);
  void PrepareDatabaseMenu(const DatabaseModel*);
  void PrepareSchemaMenu(const SchemaModel*);
  void OpenServerMemberMenu(wxMenu*, int serverItemId, const ServerMemberModel*, const ServerModel*);
  void OpenDatabaseMemberMenu(wxMenu*, int databaseItemId, const DatabaseMemberModel*, const DatabaseModel*);
  void OpenSchemaMemberMenu(wxMenu*, int schemaItemId, const SchemaMemberModel*, const DatabaseModel*);

  void AppendRoleItems(const ServerModel *serverModel, wxTreeItemId serverItem);
  void AppendDatabaseItems(const ServerModel*, wxTreeItemId parent, const std::vector<const DatabaseModel*> &database);
  void AppendTablespaceItems(const ServerModel*, wxTreeItemId parent, const std::vector<const TablespaceModel*> &database);

  void AppendDivision(const DatabaseModel *db, std::vector<const SchemaMemberModel*> &members, bool includeEmptySchemas, wxTreeItemId parentItem);
  void AppendSchemaMembers(const DatabaseModel *db, wxTreeItemId parent, bool createSchemaItem, const SchemaModel& schema, const std::vector<const SchemaMemberModel*> &members);
  void AppendEmptySchema(const DatabaseModel *db, wxTreeItemId parent, const SchemaModel& schema);
  template<typename T>
  void AppendSchemaMemberItems(const DatabaseModel *db, wxTreeItemId parent, bool qualifyNames, bool fold, const wxString& title, Oid objectClass, int img, const std::vector<const SchemaMemberModel*> &members);
  template<typename T>
  wxString FormatSchemaMemberItemName(const DatabaseModel *db, bool qualifyNames, const T& member);
  wxString FormatOperandType(const DatabaseModel* db, const OperatorModel& op, Oid typeOid);

  void DivideSchemaMembers(std::vector<const SchemaMemberModel*> &members, std::vector<const SchemaMemberModel*> &userDivision, std::vector<const SchemaMemberModel*> &systemDivision, std::map<wxString, std::vector<const SchemaMemberModel*> > &extensionDivisions);

  friend class SystemSchemasLoader;
  friend class ScriptWork;
  friend class AfterDatabaseCreated;

  DECLARE_SCRIPT_HANDLERS(Database, Create);
  DECLARE_SCRIPT_HANDLERS(Database, Alter);
  DECLARE_SCRIPT_HANDLERS(Database, Drop);
  DECLARE_SCRIPT_HANDLERS(Table, Create);
  DECLARE_SCRIPT_HANDLERS(Table, Drop);
  DECLARE_SCRIPT_HANDLERS(Table, Select);
  DECLARE_SCRIPT_HANDLERS(Table, Insert);
  DECLARE_SCRIPT_HANDLERS(Table, Update);
  DECLARE_SCRIPT_HANDLERS(Table, Delete);
  DECLARE_SCRIPT_HANDLERS(Index, Create);
  DECLARE_SCRIPT_HANDLERS(Index, Alter);
  DECLARE_SCRIPT_HANDLERS(Index, Drop);
  DECLARE_SCRIPT_HANDLERS(Schema, Create);
  DECLARE_SCRIPT_HANDLERS(Schema, Drop);
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
  DECLARE_SCRIPT_HANDLERS(TextSearchDictionary, Create);
  DECLARE_SCRIPT_HANDLERS(TextSearchDictionary, Alter);
  DECLARE_SCRIPT_HANDLERS(TextSearchDictionary, Drop);
  DECLARE_SCRIPT_HANDLERS(TextSearchParser, Create);
  DECLARE_SCRIPT_HANDLERS(TextSearchParser, Alter);
  DECLARE_SCRIPT_HANDLERS(TextSearchParser, Drop);
  DECLARE_SCRIPT_HANDLERS(TextSearchTemplate, Create);
  DECLARE_SCRIPT_HANDLERS(TextSearchTemplate, Alter);
  DECLARE_SCRIPT_HANDLERS(TextSearchTemplate, Drop);
  DECLARE_SCRIPT_HANDLERS(TextSearchConfiguration, Create);
  DECLARE_SCRIPT_HANDLERS(TextSearchConfiguration, Alter);
  DECLARE_SCRIPT_HANDLERS(TextSearchConfiguration, Drop);
  DECLARE_SCRIPT_HANDLERS(Extension, Create);
  DECLARE_SCRIPT_HANDLERS(Extension, Alter);
  DECLARE_SCRIPT_HANDLERS(Extension, Drop);
  DECLARE_SCRIPT_HANDLERS(Role, Create);
  DECLARE_SCRIPT_HANDLERS(Role, Alter);
  DECLARE_SCRIPT_HANDLERS(Role, Drop);
  DECLARE_SCRIPT_HANDLERS(Tablespace, Create);
  DECLARE_SCRIPT_HANDLERS(Tablespace, Alter);
  DECLARE_SCRIPT_HANDLERS(Tablespace, Drop);

  // context menus
  wxMenu *serverMenu;
  wxMenu *databaseMenu;
  wxMenu *tableMenu;
  wxMenu *viewMenu;
  wxMenu *sequenceMenu;
  wxMenu *functionMenu;
  wxMenu *schemaMenu;
  wxMenu *extensionMenu;
  wxMenu *textSearchParserMenu;
  wxMenu *textSearchTemplateMenu;
  wxMenu *textSearchDictionaryMenu;
  wxMenu *textSearchConfigurationMenu;
  wxMenu *indexMenu;
  wxMenu *roleMenu;
  wxMenu *tablespaceMenu;

  // remember what was the context for a context menu
  ObjectModelReference contextMenuRef;
  wxTreeItemId contextMenuItem;
  ObjectModelReference FindContextSchema();

  // remember which server/database was last mentioned in an object-selected event
  ObjectModelReference selectedRef;
  void UpdateSelectedDatabase();

  // see constructor implementation for the image list initialisation these correspond to
  static const int img_folder = 0;
  static const int img_server = 1;
  static const int img_server_encrypted = img_server + 1;
  static const int img_database = img_server_encrypted + 1;
  static const int img_table = img_database + 1;
  static const int img_unlogged_table = img_table + 1;
  static const int img_view = img_unlogged_table + 1;
  static const int img_sequence = img_view + 1;
  static const int img_function = img_sequence + 1;
  static const int img_function_aggregate = img_function + 1;
  static const int img_function_trigger = img_function_aggregate + 1;
  static const int img_function_window = img_function_trigger + 1;
  static const int img_index = img_function_window + 1;
  static const int img_index_pkey = img_index + 1;
  static const int img_index_uniq = img_index_pkey + 1;
  static const int img_column = img_index_uniq + 1;
  static const int img_column_pkey = img_column + 1;
  static const int img_role = img_column_pkey + 1;
  static const int img_text_search_template = img_role + 1;
  static const int img_text_search_parser = img_text_search_template + 1;
  static const int img_text_search_dictionary = img_text_search_parser + 1;
  static const int img_text_search_configuration = img_text_search_dictionary + 1;

  void RegisterSymbolItem(const ObjectModelReference& database, Oid oid, wxTreeItemId item) { symbolTables[database][oid] = item; }
  wxTreeItemId LookupSymbolItem(const ObjectModelReference& database, Oid oid) const;

  class ModelReference : public wxTreeItemData, public ObjectModelReference {
  public:
    ModelReference(const wxString& serverId) : ObjectModelReference(serverId) {}
    ModelReference(const wxString& serverId, Oid database) : ObjectModelReference(serverId, database) {}
    ModelReference(const wxString& serverId, Oid regclass, Oid oid) : ObjectModelReference(serverId, regclass, oid) {}
    ModelReference(const ObjectModelReference& database, Oid regclass, Oid oid, int subid = 0) : ObjectModelReference(database, regclass, oid, subid) {}
  };

  ModelReference FindItemContext(const wxTreeItemId &item) const;

  std::map< ObjectModelReference, std::map< Oid, wxTreeItemId > > symbolTables;
  std::map< ObjectModelReference, wxTreeItemId > databaseItems;

  wxString GetToolTipText(const wxTreeItemId& itemId) const;

  void SaveExpandedObjects(wxTreeItemId, std::vector<ObjectModelReference>&);
  void RestoreExpandedObjects(wxTreeItemId, const std::vector<ObjectModelReference>&);

class ServerWorkLauncher : public WorkLauncher {
public:
  ServerWorkLauncher(ObjectBrowser& ob, const ObjectModelReference& adminDatabaseRef) : ob(ob), databaseRef(adminDatabaseRef) {}

  void DoWork(ObjectBrowserManagedWork *work)
  {
    GetServerModel().SubmitWork(work);
  }

  ObjectModelReference GetDatabaseRef() const
  {
    return databaseRef;
  }

  ServerConnection GetServerConnection() const
  {
    return GetServerModel().conninfo;
  }

  wxString GetDatabaseName() const
  {
    return GetServerModel().GlobalDbName();
  }
private:
  ServerModel& GetServerModel() const
  {
    ServerModel *server = ob.Model().FindServer(databaseRef.GetServerId());
    wxASSERT(server != NULL);
    return *server;
  }
  ObjectBrowser& ob;
  const ObjectModelReference databaseRef;
};

class DatabaseWorkLauncher : public WorkLauncher {
public:
  DatabaseWorkLauncher(ObjectBrowser& ob, const ObjectModelReference& databaseRef) : ob(ob), databaseRef(databaseRef) {}

  void DoWork(ObjectBrowserManagedWork *work)
  {
    GetDatabaseModel().SubmitWork(work);
  }

  ObjectModelReference GetDatabaseRef() const
  {
    return databaseRef;
  }

  ServerConnection GetServerConnection() const
  {
    return GetServerModel().conninfo;
  }

  wxString GetDatabaseName() const
  {
    return GetServerModel().GlobalDbName();
  }
private:
  DatabaseModel& GetDatabaseModel() const
  {
    DatabaseModel *db = ob.Model().FindDatabase(databaseRef);
    wxASSERT(db != NULL);
    return *db;
  }
  ServerModel& GetServerModel() const
  {
    ServerModel *server = ob.Model().FindServer(databaseRef.GetServerId());
    wxASSERT(server != NULL);
    return *server;
  }
  ObjectBrowser& ob;
  const ObjectModelReference databaseRef;
};


};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
