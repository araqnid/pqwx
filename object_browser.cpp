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
#include "object_browser_sql.h"
#include "object_finder.h"
#include "pqwx_frame.h"
#include "catalogue_index.h"
#include "object_browser_model.h"
#include "object_browser_database_work.h"

#define BIND_SCRIPT_HANDLERS(menu, mode) \
  EVT_MENU(XRCID(#menu "Menu_Script" #mode "Window"), ObjectBrowser::On##menu##MenuScript##mode##Window) \
  EVT_MENU(XRCID(#menu "Menu_Script" #mode "File"), ObjectBrowser::On##menu##MenuScript##mode##File) \
  EVT_MENU(XRCID(#menu "Menu_Script" #mode "Clipboard"), ObjectBrowser::On##menu##MenuScript##mode##Clipboard)

BEGIN_EVENT_TABLE(ObjectBrowser, wxTreeCtrl)
  EVT_TREE_ITEM_EXPANDING(Pqwx_ObjectBrowser, ObjectBrowser::BeforeExpand)
  EVT_TREE_ITEM_GETTOOLTIP(Pqwx_ObjectBrowser, ObjectBrowser::OnGetTooltip)
  EVT_TREE_ITEM_RIGHT_CLICK(Pqwx_ObjectBrowser, ObjectBrowser::OnItemRightClick)
  EVT_COMMAND(EVENT_WORK_FINISHED, wxEVT_COMMAND_TEXT_UPDATED, ObjectBrowser::OnWorkFinished)

  EVT_MENU(XRCID("ServerMenu_Disconnect"), ObjectBrowser::OnServerMenuDisconnect)
  EVT_MENU(XRCID("ServerMenu_Properties"), ObjectBrowser::OnServerMenuProperties)
  EVT_MENU(XRCID("ServerMenu_Refresh"), ObjectBrowser::OnServerMenuRefresh)

  EVT_MENU(XRCID("DatabaseMenu_Refresh"), ObjectBrowser::OnDatabaseMenuRefresh)
  EVT_MENU(XRCID("DatabaseMenu_Properties"), ObjectBrowser::OnDatabaseMenuProperties)
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

const VersionedSql& ObjectBrowser::GetSqlDictionary() {
  static ObjectBrowserSql dict;
  return dict;
}

class LazyLoader : public wxTreeItemData {
public:
  virtual void load(wxTreeItemId parent) = 0;
};

class DatabaseLoader : public LazyLoader {
public:
  DatabaseLoader(ObjectBrowser *ob, DatabaseModel *db) : ob(ob), db(db) {}

  void load(wxTreeItemId parent) {
    ob->LoadDatabase(parent, db);
  }
  
private:
  DatabaseModel *db;
  ObjectBrowser *ob;
};

class RelationLoader : public LazyLoader {
public:
  RelationLoader(ObjectBrowser *ob, RelationModel *rel) : ob(ob), rel(rel) {}

  void load(wxTreeItemId parent) {
    ob->LoadRelation(parent, rel);
  }
  
private:
  RelationModel *rel;
  ObjectBrowser *ob;
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
  wxImageList *images = new wxImageList();
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_table.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_view.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_sequence.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_function.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_function_aggregate.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_function_trigger.png")));
  images->Add(LoadVFSImage(_T("memory:ObjectFinder/icon_function_window.png")));
  AssignImageList(images);
}

void ObjectBrowser::AddServerConnection(ServerConnection *server, DatabaseConnection *db) {
  wxString serverId = server->Identification();
  for (list<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    ServerModel *serverModel = *iter;
    if (serverModel->conn->Identification().IsSameAs(serverId)) {
      wxLogDebug(_T("Ignoring server connection already registered in object browser: %s"), serverId.c_str());
      if (db != NULL) {
	db->CloseSync();
	delete db;
      }
      return;
    }
  }

  ServerModel *serverModel = db ? new ServerModel(db) : new ServerModel();
  serverModel->conn = server;
  servers.push_back(serverModel);

  // setting the text twice is a bug workaround for wx 2.8
  // see http://trac.wxwidgets.org/ticket/10085
  wxLogDebug(_T("Connection added to object browser: %s"), server->Identification().c_str());
  wxTreeItemId serverItem = AppendItem(GetRootItem(), server->Identification());
  SetItemText(serverItem, server->Identification());
  SetItemData(serverItem, serverModel);

  RefreshDatabaseList(serverItem);
}

DatabaseConnection *ObjectBrowser::GetServerAdminConnection(ServerModel *server) {
  return GetDatabaseConnection(server, server->conn->globalDbName);
}

DatabaseConnection *ObjectBrowser::GetDatabaseConnection(ServerModel *server, const wxString &dbname) {
  DatabaseConnection *db = server->connections[dbname];
  if (db == NULL) {
    wxLogDebug(_T("Allocating connection to %s database %s"), server->conn->Identification().c_str(), dbname.c_str());
#if PG_VERSION_NUM >= 90000
    db = new DatabaseConnection(server->conn, dbname, _("Object Browser"));
#else
    db = new DatabaseConnection(server->conn, dbname);
#endif
    server->connections[dbname] = db;
  }
  return db;
}

void ObjectBrowser::Dispose() {
  wxLogDebug(_T("Disposing of ObjectBrowser"));
  for (list<ServerModel*>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    ServerModel *server = *iter;
    server->Dispose();
  }
}

void ObjectBrowser::RefreshDatabaseList(wxTreeItemId serverItem) {
  ServerModel *serverModel = dynamic_cast<ServerModel*>(GetItemData(serverItem));
  SubmitServerWork(serverModel, new RefreshDatabaseListWork(serverModel, serverItem));
}

void ObjectBrowser::SubmitServerWork(ServerModel *serverModel, ObjectBrowserWork *work) {
  ConnectAndAddWork(serverModel, GetServerAdminConnection(serverModel), work);
}

void ObjectBrowser::ConnectAndAddWork(ServerModel *serverModel, DatabaseConnection *db, ObjectBrowserWork *work) {
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

void ObjectBrowser::BeforeExpand(wxTreeEvent &event) {
  wxTreeItemIdValue cookie;
  wxTreeItemId expandingItem = event.GetItem();
  wxTreeItemId firstChildItem = GetFirstChild(expandingItem, cookie);
  wxTreeItemData *itemData = GetItemData(firstChildItem);
  if (itemData == NULL) return;
  LazyLoader *lazyLoader = dynamic_cast<LazyLoader*>(itemData);
  if (lazyLoader != NULL) {
    lazyLoader->load(expandingItem);
    wxString expandingItemText = GetItemText(expandingItem);
    SetItemText(expandingItem, expandingItemText + _(" (loading...)"));
    Delete(firstChildItem);
    // veto expand event, for lazy loader to complete
    event.Veto();
  }
}

void ObjectBrowser::LoadDatabase(wxTreeItemId databaseItem, DatabaseModel *database) {
  SubmitDatabaseWork(database, new LoadDatabaseSchemaWork(database, databaseItem));
  SubmitDatabaseWork(database, new IndexDatabaseSchemaWork(database));
  SubmitDatabaseWork(database, new LoadDatabaseDescriptionsWork(database));
}

void ObjectBrowser::LoadRelation(wxTreeItemId relationItem, RelationModel *relation) {
  wxLogDebug(_T("Load data for relation %s.%s"), relation->schema.c_str(), relation->name.c_str());
  SubmitDatabaseWork(relation->database, new LoadRelationWork(relation, relationItem));
}

void ObjectBrowser::SubmitDatabaseWork(DatabaseModel *database, ObjectBrowserWork *work) {
  ConnectAndAddWork(database->server, GetDatabaseConnection(database->server, database->name), work);
}

void ObjectBrowser::FillInServer(ServerModel *serverModel, wxTreeItemId serverItem, const wxString &serverVersionString, int serverVersion, bool usingSSL) {
  serverModel->SetVersion(serverVersion);
  serverModel->usingSSL = usingSSL;
  SetItemText(serverItem, serverModel->conn->Identification() + _T(" (") + serverVersionString + _T(")") + (usingSSL ? _T(" [SSL]") : wxEmptyString));
}

static bool CollateDatabases(DatabaseModel *d1, DatabaseModel *d2) {
  return d1->name < d2->name;
}

void ObjectBrowser::FillInDatabases(ServerModel *serverModel, wxTreeItemId serverItem, vector<DatabaseModel*> &databases) {
  sort(databases.begin(), databases.end(), CollateDatabases);
  vector<DatabaseModel*> systemDatabases;
  vector<DatabaseModel*> templateDatabases;
  vector<DatabaseModel*> userDatabases;
  wxTreeItemId systemDatabasesItem = NULL;
  wxTreeItemId templateDatabasesItem = NULL;
  for (vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
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
    AppendDatabaseItems(templateDatabasesItem, templateDatabases);
  }
  if (!systemDatabases.empty()) {
    wxTreeItemId systemDatabasesItem = AppendItem(serverItem, _("System Databases"));
    AppendDatabaseItems(systemDatabasesItem, systemDatabases);
  }
}

void ObjectBrowser::AppendDatabaseItems(wxTreeItemId parentItem, vector<DatabaseModel*> &databases) {
  for (vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
    DatabaseModel *database = *iter;
    wxTreeItemId databaseItem = AppendItem(parentItem, database->name);
    SetItemData(databaseItem, database);
    if (database->IsUsable())
      SetItemData(AppendItem(databaseItem, _T("Loading...")), new DatabaseLoader(this, database));
  }
}

static bool CollateRoles(RoleModel *r1, RoleModel *r2) {
  return r1->name < r2->name;
}

void ObjectBrowser::FillInRoles(ServerModel *serverModel, wxTreeItemId serverItem, vector<RoleModel*> &roles) {
  sort(roles.begin(), roles.end(), CollateRoles);
  wxTreeItemId usersItem = AppendItem(serverItem, _("Users"));
  wxTreeItemId groupsItem = AppendItem(serverItem, _("Groups"));
  for (vector<RoleModel*>::iterator iter = roles.begin(); iter != roles.end(); iter++) {
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

void ObjectBrowser::AppendSchemaMembers(wxTreeItemId parent, bool createSchemaItem, const wxString &schemaName, const vector<SchemaMemberModel*> &members) {
  if (members.size() == 1 && members[0]->name.IsEmpty()) {
    AppendItem(parent, schemaName + _T("."));
    return;
  }

  if (createSchemaItem) {
    parent = AppendItem(parent, schemaName + _T("."));
  }

  int relationsCount = 0;
  for (vector<SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
    SchemaMemberModel *member = *iter;
    if (member->name.IsEmpty()) continue;
    RelationModel *relation = dynamic_cast<RelationModel*>(member);
    if (relation == NULL)
      continue;
    ++relationsCount;
    wxTreeItemId memberItem = AppendItem(parent, createSchemaItem ? relation->name : relation->schema + _T(".") + relation->name);
    SetItemData(memberItem, relation);
    relation->database->symbolItemLookup[relation->oid] = memberItem;
    if (relation->type == RelationModel::TABLE || relation->type == RelationModel::VIEW)
      SetItemData(AppendItem(memberItem, _("Loading...")), new RelationLoader(this, relation));
    // see images->Add calls in constructor for the list that the 2nd param of SetItemImage indexes
    switch (relation->type) {
    case RelationModel::TABLE:
      SetItemImage(memberItem, 0);
      break;
    case RelationModel::VIEW:
      SetItemImage(memberItem, 1);
      break;
    case RelationModel::SEQUENCE:
      SetItemImage(memberItem, 2);
      break;
    }
  }

  wxTreeItemId functionsItem = parent;
  if (relationsCount != 0 && createSchemaItem) {
    functionsItem = AppendItem(parent, _("Functions"));
  }
  else {
    // no relations, so add functions directly in the schema item
    functionsItem = parent;
  }

  for (vector<SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
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
      SetItemImage(memberItem, 3);
      break;
    case FunctionModel::AGGREGATE:
      SetItemImage(memberItem, 4);
      break;
    case FunctionModel::TRIGGER:
      SetItemImage(memberItem, 5);
      break;
    case FunctionModel::WINDOW:
      SetItemImage(memberItem, 6);
      break;
    }
  }
}

void ObjectBrowser::AppendDivision(vector<SchemaMemberModel*> &members, wxTreeItemId parentItem) {
  map<wxString, vector<SchemaMemberModel*> > schemas;

  for (vector<SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
    SchemaMemberModel *member = *iter;
    schemas[member->schema].push_back(member);
  }

  bool foldSchemas = members.size() > 50 && schemas.size() > 1;
  for (map<wxString, vector<SchemaMemberModel*> >::iterator iter = schemas.begin(); iter != schemas.end(); iter++) {
    AppendSchemaMembers(parentItem, foldSchemas && iter->second.size() > 1, iter->first, iter->second);
  }
}

void ObjectBrowser::FillInDatabaseSchema(DatabaseModel *databaseModel, wxTreeItemId databaseItem) {
  databaseModel->loaded = true;

  DatabaseModel::Divisions divisions = databaseModel->DivideSchemaMembers();

  AppendDivision(divisions.userDivision, databaseItem);

  if (!divisions.extensionDivisions.empty()) {
    wxTreeItemId extensionsItem = AppendItem(databaseItem, _("Extensions"));
    for (map<wxString, vector<SchemaMemberModel*> >::iterator iter = divisions.extensionDivisions.begin(); iter != divisions.extensionDivisions.end(); iter++) {
      wxTreeItemId extensionItem = AppendItem(extensionsItem, iter->first);
      AppendDivision(iter->second, extensionItem);
    }
  }

  wxTreeItemId systemDivisionItem = AppendItem(databaseItem, _("System schemas"));
  AppendDivision(divisions.systemDivision, systemDivisionItem);
}

void ObjectBrowser::FillInRelation(RelationModel *relation, wxTreeItemId relationItem, vector<ColumnModel*> &columns, vector<IndexModel*> &indices, vector<TriggerModel*> &triggers) {
  for (vector<ColumnModel*>::iterator iter = columns.begin(); iter != columns.end(); iter++) {
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
    for (vector<IndexModel*>::iterator iter = indices.begin(); iter != indices.end(); iter++) {
      wxTreeItemId indexItem = AppendItem(indicesItem, (*iter)->name);
      SetItemData(indexItem, *iter);
    }
  }

  if (!triggers.empty()) {
    wxTreeItemId triggersItem = AppendItem(relationItem, _("Triggers"));
    for (vector<TriggerModel*>::iterator iter = triggers.begin(); iter != triggers.end(); iter++) {
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
  if (selected == GetRootItem()) {
    wxLogDebug(_T("selected item was root -- nothing is selected"));
    return;
  }
  wxTreeItemId cursor = selected;
  do {
    wxTreeItemId parent = GetItemParent(cursor);
    if (parent == GetRootItem())
      break;
    cursor = parent;
  } while (1);

  wxTreeItemData *data = GetItemData(cursor);
  wxASSERT(data != NULL);

  // in one of the top-level items, we'd better have a server model
  ServerModel *server = dynamic_cast<ServerModel*>(data);
  wxASSERT(server != NULL);

  wxLogDebug(_T("Disconnect: %s (menubar)"), server->conn->Identification().c_str());
  servers.remove(server);
  server->Dispose(); // still does nasty synchronous disconnect for now
  Delete(cursor);
}

class ZoomToFoundObjectOnCompletion : public ObjectFinder::Completion {
public:
  ZoomToFoundObjectOnCompletion(ObjectBrowser *ob, DatabaseModel *database) : ob(ob), database(database) {}
  void OnObjectChosen(const CatalogueIndex::Document *document) {
    ob->ZoomToFoundObject(database, document);
  }
  void OnCancelled() {
  }
private:
  DatabaseModel *database;
  ObjectBrowser *ob;
};

void ObjectBrowser::FindObject() {
  wxTreeItemId selected = GetSelection();
  if (!selected.IsOk()) {
    wxLogDebug(_T("selected item was not ok -- nothing is selected?"));
    return;
  }
  if (selected == GetRootItem()) {
    wxLogDebug(_T("selected item was root -- nothing is selected"));
    return;
  }
  wxTreeItemId cursor = selected;
  wxTreeItemId previous;
  do {
    wxTreeItemId parent = GetItemParent(cursor);
    if (parent == GetRootItem())
      break;
    previous = cursor;
    cursor = parent;
  } while (1);
  if (!previous.IsOk()) {
    wxLogDebug(_T("selection is above database level?"));
    return;
  }

  wxTreeItemData *data = GetItemData(previous);
  wxASSERT(data != NULL);

  DatabaseModel *database = dynamic_cast<DatabaseModel*>(data);
  wxASSERT(database != NULL);

  wxLogDebug(_T("Find object in %s %s"), database->server->conn->Identification().c_str(), database->name.c_str());

  // TODO - should be able to open the dialogue and have it start loading this if it's not already done
  if (!database->loaded) {
    wxMessageBox(_T("Database not loaded yet (expand the tree node)"));
    return;
  }
  wxASSERT(database->catalogueIndex != NULL);

  ObjectFinder *finder = new ObjectFinder(NULL, new ZoomToFoundObjectOnCompletion(this, database), database->catalogueIndex);
  finder->Show();
  finder->SetFocus();
}

void ObjectBrowser::ZoomToFoundObject(DatabaseModel *database, const CatalogueIndex::Document *document) {
  wxLogDebug(_T("Zoom to found object \"%s\" in database \"%s\" of \"%s\""), document->symbol.c_str(), database->name.c_str(), database->server->conn->Identification().c_str());
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

void ObjectBrowser::OnServerMenuDisconnect(wxCommandEvent &event) {
  wxLogDebug(_T("Disconnect: %s (context menu)"), contextMenuServer->conn->Identification().c_str());
  servers.remove(contextMenuServer);
  contextMenuServer->Dispose();
  Delete(contextMenuItem);
}

void ObjectBrowser::OnServerMenuRefresh(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Refresh server: ") + contextMenuServer->conn->Identification());
}

void ObjectBrowser::OnServerMenuProperties(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Show server properties: ") + contextMenuServer->conn->Identification());
}

void ObjectBrowser::OnDatabaseMenuRefresh(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Refresh database: ") + contextMenuDatabase->server->conn->Identification() + _T(" ") + contextMenuDatabase->name);
}

void ObjectBrowser::OnDatabaseMenuProperties(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Show database properties: ") + contextMenuDatabase->server->conn->Identification() + _T(" ") + contextMenuDatabase->name);
}
