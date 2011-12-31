#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <algorithm>
#include <set>
#include "wx/imaglist.h"
#include "object_browser.h"
#include "object_finder.h"
#include "pqwx_frame.h"
#include "catalogue_index.h"
#include "database_work.h"
#include "object_browser_model.h"
#include "object_browser_database_work.h"
#include "dependencies_view.h"

inline void ObjectBrowser::SubmitServerWork(ServerModel *server, ObjectBrowserWork *work) {
  ConnectAndAddWork(server->GetServerAdminConnection(), work);
}

inline void ObjectBrowser::SubmitDatabaseWork(DatabaseModel *database, ObjectBrowserWork *work) {
  ConnectAndAddWork(database->GetDatabaseConnection(), work);
}

#define BIND_SCRIPT_HANDLERS(menu, mode) \
  EVT_MENU(XRCID(#menu "Menu_Script" #mode "Window"), ObjectBrowser::On##menu##MenuScript##mode##Window) \
  EVT_MENU(XRCID(#menu "Menu_Script" #mode "File"), ObjectBrowser::On##menu##MenuScript##mode##File) \
  EVT_MENU(XRCID(#menu "Menu_Script" #mode "Clipboard"), ObjectBrowser::On##menu##MenuScript##mode##Clipboard)

BEGIN_EVENT_TABLE(ObjectBrowser, wxTreeCtrl)
  EVT_TREE_ITEM_EXPANDING(Pqwx_ObjectBrowser, ObjectBrowser::BeforeExpand)
  EVT_TREE_ITEM_GETTOOLTIP(Pqwx_ObjectBrowser, ObjectBrowser::OnGetTooltip)
  EVT_TREE_ITEM_RIGHT_CLICK(Pqwx_ObjectBrowser, ObjectBrowser::OnItemRightClick)
  EVT_TREE_SEL_CHANGED(Pqwx_ObjectBrowser, ObjectBrowser::OnItemSelected)
  EVT_SET_FOCUS(ObjectBrowser::OnSetFocus)
  EVT_COMMAND(EVENT_WORK_FINISHED, wxEVT_COMMAND_TEXT_UPDATED, ObjectBrowser::OnWorkFinished)

  EVT_MENU(XRCID("ServerMenu_Disconnect"), ObjectBrowser::OnServerMenuDisconnect)
  EVT_MENU(XRCID("ServerMenu_Properties"), ObjectBrowser::OnServerMenuProperties)
  EVT_MENU(XRCID("ServerMenu_Refresh"), ObjectBrowser::OnServerMenuRefresh)

  EVT_MENU(XRCID("DatabaseMenu_Refresh"), ObjectBrowser::OnDatabaseMenuRefresh)
  EVT_MENU(XRCID("DatabaseMenu_Properties"), ObjectBrowser::OnDatabaseMenuProperties)
  EVT_MENU(XRCID("DatabaseMenu_ViewDependencies"), ObjectBrowser::OnDatabaseMenuViewDependencies)
  EVT_MENU(XRCID("TableMenu_ViewDependencies"), ObjectBrowser::OnRelationMenuViewDependencies)
  EVT_MENU(XRCID("ViewMenu_ViewDependencies"), ObjectBrowser::OnRelationMenuViewDependencies)
  EVT_MENU(XRCID("SequenceMenu_ViewDependencies"), ObjectBrowser::OnRelationMenuViewDependencies)
  EVT_MENU(XRCID("FunctionMenu_ViewDependencies"), ObjectBrowser::OnFunctionMenuViewDependencies)
  BIND_SCRIPT_HANDLERS(Database, Create)
  BIND_SCRIPT_HANDLERS(Database, Alter)
  BIND_SCRIPT_HANDLERS(Database, Drop)
  BIND_SCRIPT_HANDLERS(Table, Create)
  BIND_SCRIPT_HANDLERS(Table, Drop)
  BIND_SCRIPT_HANDLERS(Table, Select)
  BIND_SCRIPT_HANDLERS(Table, Insert)
  BIND_SCRIPT_HANDLERS(Table, Update)
  BIND_SCRIPT_HANDLERS(Table, Delete)
  BIND_SCRIPT_HANDLERS(View, Create)
  BIND_SCRIPT_HANDLERS(View, Alter)
  BIND_SCRIPT_HANDLERS(View, Drop)
  BIND_SCRIPT_HANDLERS(View, Select)
  BIND_SCRIPT_HANDLERS(Sequence, Create)
  BIND_SCRIPT_HANDLERS(Sequence, Alter)
  BIND_SCRIPT_HANDLERS(Sequence, Drop)
  BIND_SCRIPT_HANDLERS(Function, Create)
  BIND_SCRIPT_HANDLERS(Function, Alter)
  BIND_SCRIPT_HANDLERS(Function, Drop)
  BIND_SCRIPT_HANDLERS(Function, Select)
END_EVENT_TABLE()

#define IMPLEMENT_SCRIPT_HANDLER(menu, mode, field, output)		\
void ObjectBrowser::On##menu##MenuScript##mode##output(wxCommandEvent &event) { \
  SubmitDatabaseWork(contextMenuDatabase, new menu##ScriptWork(field, ScriptWork::mode, ScriptWork::output)); \
}

#define IMPLEMENT_SCRIPT_HANDLERS(menu, mode, field) \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, field, Window) \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, field, File)    \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, field, Clipboard)

IMPLEMENT_SCRIPT_HANDLERS(Database, Create, contextMenuDatabase)
IMPLEMENT_SCRIPT_HANDLERS(Database, Alter, contextMenuDatabase)
IMPLEMENT_SCRIPT_HANDLERS(Database, Drop, contextMenuDatabase)
IMPLEMENT_SCRIPT_HANDLERS(Table, Create, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(Table, Drop, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(Table, Select, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(Table, Insert, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(Table, Update, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(Table, Delete, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(View, Create, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(View, Alter, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(View, Drop, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(View, Select, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(Sequence, Create, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(Sequence, Alter, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(Sequence, Drop, contextMenuRelation)
IMPLEMENT_SCRIPT_HANDLERS(Function, Create, contextMenuFunction)
IMPLEMENT_SCRIPT_HANDLERS(Function, Alter, contextMenuFunction)
IMPLEMENT_SCRIPT_HANDLERS(Function, Drop, contextMenuFunction)
IMPLEMENT_SCRIPT_HANDLERS(Function, Select, contextMenuFunction)

DEFINE_LOCAL_EVENT_TYPE(PQWX_SCRIPT_TO_WINDOW)
DEFINE_LOCAL_EVENT_TYPE(PQWX_OBJECT_SELECTED)

class DatabaseLoader : public LazyLoader {
public:
  DatabaseLoader(ObjectBrowser *ob, DatabaseModel *db) : db(db), ob(ob) {}

  bool load(wxTreeItemId parent) {
    if (!db->loaded) {
      ob->LoadDatabase(parent, db);
      return true;
    }
    return false;
  }
  
private:
  DatabaseModel *db;
  ObjectBrowser *ob;
};

class RelationLoader : public LazyLoader {
public:
  RelationLoader(ObjectBrowser *ob, RelationModel *rel) : rel(rel), ob(ob) {}

  bool load(wxTreeItemId parent) {
    ob->LoadRelation(parent, rel);
    return true;
  }
  
private:
  RelationModel *rel;
  ObjectBrowser *ob;
};

class SystemSchemasLoader : public LazyLoader {
public:
  SystemSchemasLoader(ObjectBrowser *ob, DatabaseModel *db, std::vector<SchemaMemberModel*> division) : ob(ob), db(db), division(division) {}

  bool load(wxTreeItemId parent) {
    ob->AppendDivision(division, parent);
    return false;
  }

private:
  ObjectBrowser *ob;
  DatabaseModel *db;
  std::vector<SchemaMemberModel*> division;
};

static wxImage LoadVFSImage(const wxString &vfilename) {
  wxFileSystem fs;
  wxFSFile *fsfile = fs.OpenFile(vfilename);
  wxASSERT_MSG(fsfile != NULL, vfilename);
  wxImage im;
  wxInputStream *stream = fsfile->GetStream();
  im.LoadFile(*stream, fsfile->GetMimeType());
  delete fsfile;
  return im;
}

ObjectBrowser::ObjectBrowser(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style) : wxTreeCtrl(parent, id, pos, size, style) {
  AddRoot(_T("root"));
  serverMenu = wxXmlResource::Get()->LoadMenu(_T("ServerMenu"));
  databaseMenu = wxXmlResource::Get()->LoadMenu(_T("DatabaseMenu"));
  tableMenu = wxXmlResource::Get()->LoadMenu(_T("TableMenu"));
  viewMenu = wxXmlResource::Get()->LoadMenu(_T("ViewMenu"));
  sequenceMenu = wxXmlResource::Get()->LoadMenu(_T("SequenceMenu"));
  functionMenu = wxXmlResource::Get()->LoadMenu(_T("FunctionMenu"));
  wxImageList *images = new wxImageList(13, 13, true);
  images->Add(LoadVFSImage(_T("memory:ObjectBrowser/icon_folder.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectBrowser/icon_server.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectBrowser/icon_database.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_table.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_view.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_sequence.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_function.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_function_aggregate.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_function_trigger.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_function_window.png")));
  AssignImageList(images);
  currentlySelected = false;
}

void ObjectBrowser::AddServerConnection(ServerConnection *server, DatabaseConnection *db) {
  wxString serverId = server->Identification();
  for (std::list<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    ServerModel *serverModel = *iter;
    if (serverModel->Identification() == serverId) {
      wxLogDebug(_T("Ignoring server connection already registered in object browser: %s"), serverId.c_str());
      if (db != NULL) {
	db->CloseSync();
	delete db;
      }
      return;
    }
  }

  ServerModel *serverModel = db ? new ServerModel(server, db) : new ServerModel(server);
  servers.push_back(serverModel);

  // setting the text twice is a bug workaround for wx 2.8
  // see http://trac.wxwidgets.org/ticket/10085
  wxLogDebug(_T("Connection added to object browser: %s"), server->Identification().c_str());
  wxTreeItemId serverItem = AppendItem(GetRootItem(), server->Identification());
  SetItemText(serverItem, server->Identification());
  SetItemData(serverItem, serverModel);
  SetItemImage(serverItem, img_server);

  RefreshDatabaseList(serverItem);
}

DatabaseConnection* ServerModel::GetDatabaseConnection(const wxString &dbname) {
  std::map<wxString, DatabaseConnection*>::const_iterator iter = connections.find(dbname);
  if (iter != connections.end()) {
    DatabaseConnection *db = iter->second;
    if (db->IsConnected()) {
      wxLogDebug(_T("Using existing connection %s"), db->Identification().c_str());
      return db;
    }
    wxLogDebug(_T("Cleaning stale connection %s"), db->Identification().c_str());
    db->Dispose();
    delete db;
    connections.erase(dbname);
  }
#if PG_VERSION_NUM >= 90000
  DatabaseConnection *db = new DatabaseConnection(conn, dbname, _("Object Browser"));
#else
  DatabaseConnection *db = new DatabaseConnection(conn, dbname);
#endif
  wxLogDebug(_T("Allocating connection to %s"), db->Identification().c_str());
  for (std::map<wxString, DatabaseConnection*>::const_iterator iter = connections.begin(); iter != connections.end(); iter++) {
    if (iter->second->IsConnected()) {
      wxLogDebug(_T(" Closing existing connection to %s"), iter->second->Identification().c_str());
      iter->second->BeginDisconnection();
    }
  }
  connections[dbname] = db;
  return db;
}

void ObjectBrowser::Dispose() {
  wxLogDebug(_T("Disposing of ObjectBrowser- sending disconnection request to all server databases"));
  std::vector<DatabaseConnection*> disconnecting;
  for (std::list<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    ServerModel *server = *iter;
    server->BeginDisconnectAll(disconnecting);
  }
  wxLogDebug(_T("Disposing of ObjectBrowser- waiting for %lu database connections to terminate"), disconnecting.size());
  for (std::vector<DatabaseConnection*>::const_iterator iter = disconnecting.begin(); iter != disconnecting.end(); iter++) {
    DatabaseConnection *db = *iter;
    wxLogDebug(_T(" Waiting for database connection %s to exit"), db->Identification().c_str());
    db->WaitUntilClosed();
  }
  wxLogDebug(_T("Disposing of ObjectBrowser- disposing of servers"));
  for (std::list<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    ServerModel *server = *iter;
    server->Dispose();
  }
  wxLogDebug(_T("Disposing of ObjectBrowser- clearing server list"));
  servers.clear();
}

void ServerModel::BeginDisconnectAll(std::vector<DatabaseConnection*> &disconnecting) {
  for (std::map<wxString, DatabaseConnection*>::iterator iter = connections.begin(); iter != connections.end(); iter++) {
    DatabaseConnection *db = iter->second;
    if (db->BeginDisconnection()) {
      wxLogDebug(_T(" Sent disconnect request to %s"), db->Identification().c_str());
      disconnecting.push_back(db);
    }
    else {
      wxLogDebug(_T(" Already disconnected from %s"), db->Identification().c_str());
    }
  }
}

void ServerModel::Dispose() {
  wxLogDebug(_T("Disposing of server %s"), conn->Identification().c_str());
  std::vector<DatabaseConnection*> disconnecting;
  BeginDisconnectAll(disconnecting);
  for (std::vector<DatabaseConnection*>::const_iterator iter = disconnecting.begin(); iter != disconnecting.end(); iter++) {
    DatabaseConnection *db = *iter;
    wxLogDebug(_T(" Waiting for database connection %s to exit"), db->Identification().c_str());
    db->WaitUntilClosed();
  }
  for (std::map<wxString, DatabaseConnection*>::iterator iter = connections.begin(); iter != connections.end(); iter++) {
    DatabaseConnection *db = iter->second;
    db->Dispose();
    delete db;
  }
  connections.clear();
}

void ObjectBrowser::RefreshDatabaseList(wxTreeItemId serverItem) {
  ServerModel *serverModel = dynamic_cast<ServerModel*>(GetItemData(serverItem));
  SubmitServerWork(serverModel, new RefreshDatabaseListWork(serverModel, serverItem));
}

void ObjectBrowser::ConnectAndAddWork(DatabaseConnection *db, ObjectBrowserWork *work) {
  // still a bodge. what if the database connection fails? need to clean up any work added in the meantime...
  if (!db->IsConnected()) {
    db->Connect();
#if PG_VERSION_NUM < 90000
    db->Relabel(_("Object Browser"));
#endif
  }
  db->AddWork(new ObjectBrowserDatabaseWork(this, work));
}

void ObjectBrowser::OnWorkFinished(wxCommandEvent &e) {
  ObjectBrowserWork *work = static_cast<ObjectBrowserWork*>(e.GetClientData());

  wxLogDebug(_T("%p: work finished"), work);
  work->LoadIntoView(this);

  delete work;
}

LazyLoader *ObjectBrowser::GetLazyLoader(wxTreeItemId item) const {
  wxTreeItemIdValue cookie;
  wxTreeItemId firstChildItem = GetFirstChild(item, cookie);
  wxTreeItemData *itemData = GetItemData(firstChildItem);
  if (itemData == NULL) return NULL;
  LazyLoader *lazyLoader = dynamic_cast<LazyLoader*>(itemData);
  if (lazyLoader != NULL) {
    return lazyLoader;
  }
  return NULL;
}

void ObjectBrowser::DeleteLazyLoader(wxTreeItemId item) {
  wxTreeItemIdValue cookie;
  wxTreeItemId firstChildItem = GetFirstChild(item, cookie);
  wxTreeItemData *itemData = GetItemData(firstChildItem);
  if (itemData == NULL) return;
  LazyLoader *lazyLoader = dynamic_cast<LazyLoader*>(itemData);
  if (lazyLoader != NULL) {
    Delete(firstChildItem);
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
    if (lazyLoader->load(expandingItem)) {
      wxString expandingItemText = GetItemText(expandingItem);
      SetItemText(expandingItem, expandingItemText + _(" (loading...)"));
      // veto expand event, for lazy loader to complete
      event.Veto();
    }
    Delete(firstChildItem);
  }
}

void ObjectBrowser::LoadDatabase(wxTreeItemId databaseItem, DatabaseModel *database, IndexSchemaCompletionCallback *indexCompletion) {
  SubmitDatabaseWork(database, new LoadDatabaseSchemaWork(database, databaseItem));
  SubmitDatabaseWork(database, new IndexDatabaseSchemaWork(database, indexCompletion));
  SubmitDatabaseWork(database, new LoadDatabaseDescriptionsWork(database));
}

void ObjectBrowser::LoadRelation(wxTreeItemId relationItem, RelationModel *relation) {
  wxLogDebug(_T("Load data for relation %s.%s"), relation->schema.c_str(), relation->name.c_str());
  SubmitDatabaseWork(relation->database, new LoadRelationWork(relation, relationItem));
}

void ObjectBrowser::FillInServer(ServerModel *serverModel, wxTreeItemId serverItem) {
  SetItemText(serverItem, serverModel->Identification() + _T(" (") + serverModel->VersionString() + _T(")") + (serverModel->IsUsingSSL() ? _T(" [SSL]") : wxEmptyString));
}

static bool CollateDatabases(DatabaseModel *d1, DatabaseModel *d2) {
  return d1->name < d2->name;
}

void ObjectBrowser::FillInDatabases(ServerModel *serverModel, wxTreeItemId serverItem, std::vector<DatabaseModel*> &databases) {
  sort(databases.begin(), databases.end(), CollateDatabases);
  std::vector<DatabaseModel*> systemDatabases;
  std::vector<DatabaseModel*> templateDatabases;
  std::vector<DatabaseModel*> userDatabases;
  for (std::vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
    DatabaseModel *database = *iter;
    if (database->IsSystem()) {
      systemDatabases.push_back(database);
    }
    else if (database->isTemplate) {
      templateDatabases.push_back(database);
    }
    else {
      userDatabases.push_back(database);
    }
  }
  if (!userDatabases.empty()) {
    AppendDatabaseItems(serverItem, userDatabases);
  }
  if (!templateDatabases.empty()) {
    wxTreeItemId templateDatabasesItem = AppendItem(serverItem, _("Template Databases"));
    SetItemImage(templateDatabasesItem, img_folder);
    AppendDatabaseItems(templateDatabasesItem, templateDatabases);
  }
  if (!systemDatabases.empty()) {
    wxTreeItemId systemDatabasesItem = AppendItem(serverItem, _("System Databases"));
    SetItemImage(systemDatabasesItem, img_folder);
    AppendDatabaseItems(systemDatabasesItem, systemDatabases);
  }
}

void ObjectBrowser::AppendDatabaseItems(wxTreeItemId parentItem, std::vector<DatabaseModel*> &databases) {
  for (std::vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
    DatabaseModel *database = *iter;
    wxTreeItemId databaseItem = AppendItem(parentItem, database->name);
    SetItemData(databaseItem, database);
    SetItemImage(databaseItem, img_database);
    if (database->IsUsable())
      SetItemData(AppendItem(databaseItem, _T("Loading...")), new DatabaseLoader(this, database));
  }
}

static bool CollateRoles(RoleModel *r1, RoleModel *r2) {
  return r1->name < r2->name;
}

void ObjectBrowser::FillInRoles(ServerModel *serverModel, wxTreeItemId serverItem, std::vector<RoleModel*> &roles) {
  sort(roles.begin(), roles.end(), CollateRoles);
  wxTreeItemId usersItem = AppendItem(serverItem, _("Users"));
  SetItemImage(usersItem, img_folder);
  wxTreeItemId groupsItem = AppendItem(serverItem, _("Groups"));
  SetItemImage(groupsItem, img_folder);
  for (std::vector<RoleModel*>::iterator iter = roles.begin(); iter != roles.end(); iter++) {
    RoleModel *role = *iter;
    wxTreeItemId roleItem;
    if (role->canLogin) {
      roleItem = AppendItem(usersItem, role->name);
    }
    else {
      roleItem = AppendItem(groupsItem, role->name);
    }
    SetItemData(roleItem, role);
  }
}

void ObjectBrowser::AppendSchemaMembers(wxTreeItemId parent, bool createSchemaItem, const wxString &schemaName, const std::vector<SchemaMemberModel*> &members) {
  if (members.size() == 1 && members[0]->name.IsEmpty()) {
    wxTreeItemId emptySchemaItem = AppendItem(parent, schemaName + _T("."));
    SetItemImage(emptySchemaItem, img_folder);
    return;
  }

  if (createSchemaItem) {
    parent = AppendItem(parent, schemaName + _T("."));
    SetItemImage(parent, img_folder);
  }

  int relationsCount = 0;
  int functionsCount = 0;
  for (std::vector<SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
    SchemaMemberModel *member = *iter;
    if (member->name.IsEmpty()) continue;
    RelationModel *relation = dynamic_cast<RelationModel*>(member);
    if (relation == NULL) {
      FunctionModel *function = dynamic_cast<FunctionModel*>(member);
      if (function != NULL) ++functionsCount;
      continue;
    }
    ++relationsCount;
    wxTreeItemId memberItem = AppendItem(parent, createSchemaItem ? relation->name : relation->schema + _T(".") + relation->name);
    SetItemData(memberItem, relation);
    relation->database->symbolItemLookup[relation->oid] = memberItem;
    if (relation->type == RelationModel::TABLE || relation->type == RelationModel::VIEW)
      SetItemData(AppendItem(memberItem, _("Loading...")), new RelationLoader(this, relation));
    switch (relation->type) {
    case RelationModel::TABLE:
      SetItemImage(memberItem, img_table);
      break;
    case RelationModel::VIEW:
      SetItemImage(memberItem, img_view);
      break;
    case RelationModel::SEQUENCE:
      SetItemImage(memberItem, img_sequence);
      break;

    }
  }

  if (functionsCount == 0)
    return;

  wxTreeItemId functionsItem = parent;
  if (relationsCount != 0 && createSchemaItem) {
    functionsItem = AppendItem(parent, _("Functions"));
    SetItemImage(functionsItem, img_folder);
  }
  else {
    // no relations, so add functions directly in the schema item
    functionsItem = parent;
  }

  for (std::vector<SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
    SchemaMemberModel *member = *iter;
    if (member->name.IsEmpty()) continue;
    FunctionModel *function = dynamic_cast<FunctionModel*>(member);
    if (function == NULL) continue;
    wxTreeItemId memberItem = AppendItem(functionsItem, createSchemaItem ? function->name + _T("(") + function->arguments + _T(")") : function->schema + _T(".") + function->name + _T("(") + function->arguments + _T(")"));
    SetItemData(memberItem, function);
    function->database->symbolItemLookup[function->oid] = memberItem;
    // see images->Add calls in constructor
    switch (function->type) {
    case FunctionModel::SCALAR:
    case FunctionModel::RECORDSET:
      SetItemImage(memberItem, img_function);
      break;
    case FunctionModel::AGGREGATE:
      SetItemImage(memberItem, img_function_aggregate);
      break;
    case FunctionModel::TRIGGER:
      SetItemImage(memberItem, img_function_trigger);
      break;
    case FunctionModel::WINDOW:
      SetItemImage(memberItem, img_function_window);
      break;
    }
  }
}

void ObjectBrowser::AppendDivision(std::vector<SchemaMemberModel*> &members, wxTreeItemId parentItem) {
  std::map<wxString, std::vector<SchemaMemberModel*> > schemas;

  for (std::vector<SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
    SchemaMemberModel *member = *iter;
    schemas[member->schema].push_back(member);
  }

  bool foldSchemas = members.size() > 50 && schemas.size() > 1;
  for (std::map<wxString, std::vector<SchemaMemberModel*> >::iterator iter = schemas.begin(); iter != schemas.end(); iter++) {
    AppendSchemaMembers(parentItem, foldSchemas && iter->second.size() > 1, iter->first, iter->second);
  }
}

void ObjectBrowser::FillInDatabaseSchema(DatabaseModel *databaseModel, wxTreeItemId databaseItem) {
  databaseModel->loaded = true;

  DatabaseModel::Divisions divisions = databaseModel->DivideSchemaMembers();

  AppendDivision(divisions.userDivision, databaseItem);

  if (!divisions.extensionDivisions.empty()) {
    wxTreeItemId extensionsItem = AppendItem(databaseItem, _("Extensions"));
    SetItemImage(extensionsItem, img_folder);
    for (std::map<wxString, std::vector<SchemaMemberModel*> >::iterator iter = divisions.extensionDivisions.begin(); iter != divisions.extensionDivisions.end(); iter++) {
      wxTreeItemId extensionItem = AppendItem(extensionsItem, iter->first);
      SetItemImage(extensionItem, img_folder);
      AppendDivision(iter->second, extensionItem);
    }
  }

  wxTreeItemId systemDivisionItem = AppendItem(databaseItem, _("System schemas"));
  SetItemImage(systemDivisionItem, img_folder);
  wxTreeItemId systemDivisionLoaderItem = AppendItem(systemDivisionItem, _T("Loading..."));
  SetItemData(systemDivisionLoaderItem, new SystemSchemasLoader(this, databaseModel, divisions.systemDivision));
}

void ObjectBrowser::FillInRelation(RelationModel *relation, wxTreeItemId relationItem, std::vector<ColumnModel*> &columns, std::vector<IndexModel*> &indices, std::vector<TriggerModel*> &triggers) {
  for (std::vector<ColumnModel*>::iterator iter = columns.begin(); iter != columns.end(); iter++) {
    ColumnModel *column = *iter;
    wxString itemText = column->name + _T(" (") + column->type;

    if (relation->type == RelationModel::TABLE) {
      if (column->nullable)
	itemText += _(", null");
      else
	itemText += _(", not null");
      if (column->hasDefault)
	itemText += _(", default");
    }

    itemText += _T(")");

    wxTreeItemId columnItem = AppendItem(relationItem, itemText);
    SetItemData(columnItem, column);
  }

  if (!indices.empty()) {
    wxTreeItemId indicesItem = AppendItem(relationItem, _("Indices"));
    SetItemImage(indicesItem, img_folder);
    for (std::vector<IndexModel*>::iterator iter = indices.begin(); iter != indices.end(); iter++) {
      wxTreeItemId indexItem = AppendItem(indicesItem, (*iter)->name);
      SetItemData(indexItem, *iter);
    }
  }

  if (!triggers.empty()) {
    wxTreeItemId triggersItem = AppendItem(relationItem, _("Triggers"));
    SetItemImage(triggersItem, img_folder);
    for (std::vector<TriggerModel*>::iterator iter = triggers.begin(); iter != triggers.end(); iter++) {
      wxTreeItemId triggerItem = AppendItem(triggersItem, (*iter)->name);
      SetItemData(triggerItem, *iter);
    }
  }
}

void ObjectBrowser::OnGetTooltip(wxTreeEvent &event) {
  wxTreeItemId item = event.GetItem();
  wxTreeItemData *itemData = GetItemData(item);
  if (itemData == NULL) return;
  ObjectModel *object = dynamic_cast<ObjectModel*>(itemData);
  if (object == NULL) return;
  event.SetToolTip(object->description);
}

void ObjectBrowser::DisconnectSelected() {
  wxTreeItemId selected = GetSelection();
  if (!selected.IsOk()) {
    wxLogDebug(_T("selected item was not ok -- nothing is selected?"));
    return;
  }
  ServerModel *server;
  DatabaseModel *database;
  FindItemContext(selected, &server, &database);
  if (server != NULL) {
    wxLogDebug(_T("Disconnect: %s (menubar)"), server->Identification().c_str());
    servers.remove(server);
    server->Dispose(); // still does nasty synchronous disconnect for now
    Delete(FindServerItem(server));
  }
}

class ZoomToFoundObjectOnCompletion : public ObjectFinder::Completion {
public:
  ZoomToFoundObjectOnCompletion(ObjectBrowser *ob, DatabaseModel *database) : database(database), ob(ob) {}
  void OnObjectChosen(const CatalogueIndex::Document *document) {
    ob->ZoomToFoundObject(database, document);
  }
  void OnCancelled() {
  }
private:
  DatabaseModel *database;
  ObjectBrowser *ob;
};

class OpenObjectFinderOnIndexSchemaCompletion : public IndexSchemaCompletionCallback {
protected:
  void Completed(ObjectBrowser *ob, DatabaseModel *db, const CatalogueIndex *catalogue) {
    ObjectFinder *finder = new ObjectFinder(NULL, new ZoomToFoundObjectOnCompletion(ob, db), catalogue);
    finder->Show();
    finder->SetFocus();
  }
};

void ObjectBrowser::FindObject() {
  wxTreeItemId selected = GetSelection();
  if (!selected.IsOk()) {
    wxLogDebug(_T("selected item was not ok -- nothing is selected?"));
    return;
  }

  ServerModel *server;
  DatabaseModel *database;
  FindItemContext(selected, &server, &database);
  if (database == NULL) {
    wxLogDebug(_T("No database selected, can't open find object here"));
  }

  wxLogDebug(_T("Find object in %s"), database->Identification().c_str());

  // TODO - should be able to open the dialogue and have it start loading this if it's not already done
  if (!database->loaded) {
    LoadDatabase(FindDatabaseItem(database), database, new OpenObjectFinderOnIndexSchemaCompletion());
    return;
  }
  wxASSERT(database->catalogueIndex != NULL);

  ObjectFinder *finder = new ObjectFinder(NULL, new ZoomToFoundObjectOnCompletion(this, database), database->catalogueIndex);
  finder->Show();
  finder->SetFocus();
}

wxTreeItemId ObjectBrowser::FindServerItem(ServerModel *server) const {
  wxTreeItemIdValue cookie;
  wxTreeItemId childItem = GetFirstChild(GetRootItem(), cookie);
  do {
    wxASSERT(childItem.IsOk());
    ServerModel *itemServer = static_cast<ServerModel*>(GetItemData(childItem));
    if (itemServer == server)
      return childItem;
    childItem = GetNextChild(GetRootItem(), cookie);
  } while (1);
}

wxTreeItemId ObjectBrowser::FindDatabaseItem(DatabaseModel *database) const {
  wxTreeItemId serverItem = FindServerItem(database->server);
  wxASSERT(serverItem.IsOk());
  wxTreeItemIdValue cookie;
  wxTreeItemId childItem = GetFirstChild(serverItem, cookie);
  do {
    wxASSERT(childItem.IsOk());
    DatabaseModel *itemDatabase = static_cast<DatabaseModel*>(GetItemData(childItem));
    if (itemDatabase == database)
      return childItem;
    childItem = GetNextChild(serverItem, cookie);
  } while (1);
}

wxTreeItemId ObjectBrowser::FindSystemSchemasItem(DatabaseModel *database) const {
  wxTreeItemId databaseItem = FindDatabaseItem(database);
  wxASSERT(databaseItem.IsOk());
  wxTreeItemIdValue cookie;
  wxTreeItemId childItem = GetFirstChild(databaseItem, cookie);
  do {
    wxASSERT(childItem.IsOk());
    // nasty
    if (GetItemText(childItem) == _("System schemas")) {
      return childItem;
    }
    childItem = GetNextChild(databaseItem, cookie);
  } while (1);
}

void ObjectBrowser::ZoomToFoundObject(DatabaseModel *database, const CatalogueIndex::Document *document) {
  wxLogDebug(_T("Zoom to found object \"%s\" in database \"%s\""), document->symbol.c_str(), database->Identification().c_str());
  if (database->symbolItemLookup.count(document->entityId) == 0) {
    wxTreeItemId item = FindSystemSchemasItem(database);
    LazyLoader *systemSchemasLoader = GetLazyLoader(item);
    if (systemSchemasLoader != NULL) {
      wxBusyCursor wait;
      wxLogDebug(_T("Forcing load of system schemas"));
      systemSchemasLoader->load(item);
      DeleteLazyLoader(item);
    }
  }
  wxASSERT(database->symbolItemLookup.count(document->entityId) > 0);
  wxTreeItemId item = database->symbolItemLookup[document->entityId];
  EnsureVisible(item);
  SelectItem(item);
  Expand(item);
}

void ObjectBrowser::OnItemRightClick(wxTreeEvent &event) {
  contextMenuItem = event.GetItem();

  wxTreeItemData *data = GetItemData(contextMenuItem);
  if (data == NULL) {
    // some day this will never happen, when every tree node has something useful
    return;
  }

  ServerModel *server = dynamic_cast<ServerModel*>(data);
  if (server != NULL) {
    contextMenuServer = server;
    PopupMenu(serverMenu);
    return;
  }

  DatabaseModel *database = dynamic_cast<DatabaseModel*>(data);
  if (database != NULL) {
    contextMenuDatabase = database;
    PopupMenu(databaseMenu);
    return;
  }

  RelationModel *relation = dynamic_cast<RelationModel*>(data);
  if (relation != NULL) {
    contextMenuDatabase = relation->database;
    contextMenuRelation = relation;
    switch (relation->type) {
    case RelationModel::TABLE:
      PopupMenu(tableMenu);
      break;
    case RelationModel::VIEW:
      PopupMenu(viewMenu);
      break;
    case RelationModel::SEQUENCE:
      PopupMenu(sequenceMenu);
      break;
    }
    return;
  }

  FunctionModel *function = dynamic_cast<FunctionModel*>(data);
  if (function != NULL) {
    contextMenuDatabase = function->database;
    contextMenuFunction = function;
    PopupMenu(functionMenu);
    return;
  }
}

void ObjectBrowser::FindItemContext(const wxTreeItemId &item, ServerModel **server, DatabaseModel **database) const
{
  *database = NULL;
  *server = NULL;
  for (wxTreeItemId cursor = item; cursor != GetRootItem(); cursor = GetItemParent(cursor)) {
    wxTreeItemData *data = GetItemData(cursor);
    if (data != NULL) {
      if (*database == NULL)
	*database = dynamic_cast<DatabaseModel*>(data);
      if (*server == NULL)
	*server = dynamic_cast<ServerModel*>(data);
    }
  }
}

void ObjectBrowser::OnItemSelected(wxTreeEvent &event)
{
  UpdateSelectedDatabase();
}

void ObjectBrowser::OnSetFocus(wxFocusEvent &event)
{
  wxTreeCtrl::OnSetFocus(event);
  UpdateSelectedDatabase();
}

void ObjectBrowser::UpdateSelectedDatabase() {
  wxTreeItemId selected = GetSelection();
  DatabaseModel *database = NULL;
  ServerModel *server = NULL;
  FindItemContext(selected, &server, &database);

  if (server == NULL) {
    if (currentlySelected) {
      currentlySelected = false;
      selectedServer = NULL;
      selectedDatabase = NULL;
    }
    return;
  }

  bool changed;
  if (!currentlySelected)
    changed = true;
  else if (database == NULL)
    changed = selectedDatabase != NULL || selectedServer != server;
  else
    changed = selectedDatabase != database;

  if (changed) {
    selectedServer = server;
    selectedDatabase = database;
    currentlySelected = true;
    wxCommandEvent evt(PQWX_OBJECT_SELECTED);
    if (database != NULL) {
      evt.SetString(database->Identification());
    }
    else {
      evt.SetString(server->Identification());
    }
    ProcessEvent(evt);
  }
}

void ObjectBrowser::OnServerMenuDisconnect(wxCommandEvent &event) {
  wxLogDebug(_T("Disconnect: %s (context menu)"), contextMenuServer->Identification().c_str());
  servers.remove(contextMenuServer);
  contextMenuServer->Dispose();
  Delete(contextMenuItem);
}

void ObjectBrowser::OnServerMenuRefresh(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Refresh server: ") + contextMenuServer->Identification());
}

void ObjectBrowser::OnServerMenuProperties(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Show server properties: ") + contextMenuServer->Identification());
}

void ObjectBrowser::OnDatabaseMenuRefresh(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Refresh database: ") + contextMenuDatabase->server->Identification() + _T(" ") + contextMenuDatabase->name);
}

void ObjectBrowser::OnDatabaseMenuProperties(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Show database properties: ") + contextMenuDatabase->server->Identification() + _T(" ") + contextMenuDatabase->name);
}

void ObjectBrowser::OnDatabaseMenuViewDependencies(wxCommandEvent &event) {
  DependenciesView *dialog = new DependenciesView(NULL, contextMenuDatabase->GetDatabaseConnection(), contextMenuDatabase->FormatName(), 1262 /* pg_database */, (Oid) contextMenuDatabase->oid, (Oid) contextMenuDatabase->oid);
  dialog->Show();
}

void ObjectBrowser::OnRelationMenuViewDependencies(wxCommandEvent &event) {
  DependenciesView *dialog = new DependenciesView(NULL, contextMenuDatabase->GetDatabaseConnection(), contextMenuRelation->FormatName(), 1259 /* pg_class */, (Oid) contextMenuRelation->oid, (Oid) contextMenuDatabase->oid);
  dialog->Show();
}

void ObjectBrowser::OnFunctionMenuViewDependencies(wxCommandEvent &event) {
  DependenciesView *dialog = new DependenciesView(NULL, contextMenuDatabase->GetDatabaseConnection(), contextMenuRelation->FormatName(), 1255 /* pg_proc */, (Oid) contextMenuFunction->oid, (Oid) contextMenuDatabase->oid);
  dialog->Show();
}
