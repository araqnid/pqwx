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
#include "wx/wupdlock.h"
#include "pqwx.h"
#include "object_browser.h"
#include "object_finder.h"
#include "pqwx_frame.h"
#include "catalogue_index.h"
#include "database_work.h"
#include "object_browser_model.h"
#include "object_browser_database_work.h"
#include "object_browser_scripts.h"
#include "dependencies_view.h"
#include "static_resources.h"

inline void ObjectBrowser::SubmitServerWork(ServerModel *server, ObjectBrowserWork *work)
{
  objectBrowserModel->SubmitServerWork(server->Identification(), work);
}

inline void ObjectBrowser::SubmitDatabaseWork(DatabaseModel *database, ObjectBrowserWork *work)
{
  objectBrowserModel->SubmitDatabaseWork(*database, work);
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

  EVT_MENU(XRCID("ServerMenu_Disconnect"), ObjectBrowser::OnServerMenuDisconnect)
  EVT_MENU(XRCID("ServerMenu_Properties"), ObjectBrowser::OnServerMenuProperties)
  EVT_MENU(XRCID("ServerMenu_Refresh"), ObjectBrowser::OnServerMenuRefresh)

  EVT_MENU(XRCID("DatabaseMenu_Refresh"), ObjectBrowser::OnDatabaseMenuRefresh)
  EVT_MENU(XRCID("DatabaseMenu_Properties"), ObjectBrowser::OnDatabaseMenuProperties)
  EVT_MENU(XRCID("DatabaseMenu_ViewDependencies"), ObjectBrowser::OnDatabaseMenuViewDependencies)
  EVT_MENU(XRCID("DatabaseMenu_Query"), ObjectBrowser::OnDatabaseMenuQuery)
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

#define IMPLEMENT_SCRIPT_HANDLER(menu, mode, database, oid, output)        \
void ObjectBrowser::On##menu##MenuScript##mode##output(wxCommandEvent &event) { \
  SubmitDatabaseWork(contextMenuDatabase, new menu##ScriptWork(database->server->conninfo, database->name, oid, ScriptWork::mode, ScriptWork::output)); \
}

#define IMPLEMENT_SCRIPT_HANDLERS(menu, mode, database, oid)        \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, database, oid, Window)        \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, database, oid, File)        \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, database, oid, Clipboard)

IMPLEMENT_SCRIPT_HANDLERS(Database, Create, contextMenuDatabase, contextMenuDatabase->oid)
IMPLEMENT_SCRIPT_HANDLERS(Database, Alter, contextMenuDatabase, contextMenuDatabase->oid)
IMPLEMENT_SCRIPT_HANDLERS(Database, Drop, contextMenuDatabase, contextMenuDatabase->oid)
IMPLEMENT_SCRIPT_HANDLERS(Table, Create, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(Table, Drop, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(Table, Select, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(Table, Insert, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(Table, Update, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(Table, Delete, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(View, Create, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(View, Alter, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(View, Drop, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(View, Select, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(Sequence, Create, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(Sequence, Alter, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(Sequence, Drop, contextMenuDatabase, contextMenuRelation->oid)
IMPLEMENT_SCRIPT_HANDLERS(Function, Create, contextMenuDatabase, contextMenuFunction->oid)
IMPLEMENT_SCRIPT_HANDLERS(Function, Alter, contextMenuDatabase, contextMenuFunction->oid)
IMPLEMENT_SCRIPT_HANDLERS(Function, Drop, contextMenuDatabase, contextMenuFunction->oid)
IMPLEMENT_SCRIPT_HANDLERS(Function, Select, contextMenuDatabase, contextMenuFunction->oid)

DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptNew)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptToWindow)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ObjectSelected)
DEFINE_LOCAL_EVENT_TYPE(PQWX_NoObjectSelected)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ObjectBrowserWorkFinished)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ObjectBrowserWorkCrashed)

class DatabaseLoader : public LazyLoader {
public:
  DatabaseLoader(ObjectBrowser *ob, const ObjectModelReference& databaseRef) : ob(ob), databaseRef(databaseRef) {}

  bool load(wxTreeItemId parent) {
    DatabaseModel *db = ob->Model()->FindDatabase(databaseRef);
    if (!db->loaded) {
      ob->LoadDatabase(databaseRef);
      return true;
    }
    return false;
  }
  
private:
  ObjectBrowser *ob;
  const ObjectModelReference databaseRef;
};

class RelationLoader : public LazyLoader {
public:
  RelationLoader(ObjectBrowser *ob, const ObjectModelReference& databaseRef, Oid oid) : ob(ob), relationRef(databaseRef, ObjectModelReference::PG_CLASS, oid) {}

  bool load(wxTreeItemId parent) {
    ob->LoadRelation(relationRef);
    return true;
  }
  
private:
  ObjectBrowser *ob;
  const ObjectModelReference relationRef;
};

class SystemSchemasLoader : public LazyLoader {
public:
  SystemSchemasLoader(ObjectBrowser *ob, const ObjectModelReference& databaseRef, std::vector<const SchemaMemberModel*> division) : ob(ob), databaseRef(databaseRef), division(division) {}

  bool load(wxTreeItemId parent) {
    DatabaseModel *db = ob->Model()->FindDatabase(databaseRef);
    wxWindowUpdateLocker noUpdates(ob);
    ob->AppendDivision(db, division, parent);
    return false;
  }

private:
  ObjectBrowser *ob;
  const ObjectModelReference databaseRef;
  std::vector<const SchemaMemberModel*> division;
};

ObjectBrowser::ObjectBrowser(ObjectBrowserModel *objectBrowserModel, wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style) : wxTreeCtrl(parent, id, pos, size, style), objectBrowserModel(objectBrowserModel) {
  AddRoot(_T("root"));
  serverMenu = wxXmlResource::Get()->LoadMenu(_T("ServerMenu"));
  databaseMenu = wxXmlResource::Get()->LoadMenu(_T("DatabaseMenu"));
  tableMenu = wxXmlResource::Get()->LoadMenu(_T("TableMenu"));
  viewMenu = wxXmlResource::Get()->LoadMenu(_T("ViewMenu"));
  sequenceMenu = wxXmlResource::Get()->LoadMenu(_T("SequenceMenu"));
  functionMenu = wxXmlResource::Get()->LoadMenu(_T("FunctionMenu"));
  wxImageList *images = new wxImageList(13, 13, true);
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_folder.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_server.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_server_encrypted.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_database.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_table.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_unlogged_table.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_view.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_sequence.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_function.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_function_aggregate.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_function_trigger.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_function_window.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_index.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_index_pkey.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_index_uniq.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_column.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_column_pkey.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_role.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_text_search_template.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_text_search_parser.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_text_search_dictionary.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectFinder/icon_text_search_configuration.png")));
  AssignImageList(images);
  currentlySelected = false;
  objectBrowserModel->RegisterView(this);
}

void ObjectBrowser::AddServerConnection(const ServerConnection& server, DatabaseConnection *db) {
  ServerModel *serverModel = objectBrowserModel->FindServer(server);
  if (serverModel != NULL) {
    wxLogDebug(_T("Ignoring server connection already registered in object browser: %s"), server.Identification().c_str());
    if (db != NULL) {
      db->CloseSync();
      delete db;
    }
    return;
  }

  serverModel = objectBrowserModel->AddServerConnection(server, db);

  // setting the text twice is a bug workaround for wx 2.8
  // see http://trac.wxwidgets.org/ticket/10085
  wxLogDebug(_T("Connection added to object browser: %s"), server.Identification().c_str());
  wxTreeItemId serverItem = AppendItem(GetRootItem(), server.Identification());
  SetItemText(serverItem, server.Identification());
  SetItemData(serverItem, new ModelReference(server.Identification()));
  SetItemImage(serverItem, img_server);
  SetFocus();

  RefreshDatabaseList(serverItem);
}

void ObjectBrowser::Dispose()
{
  objectBrowserModel->UnregisterView(this);
}

void ObjectBrowser::RefreshDatabaseList(wxTreeItemId serverItem) {
  ModelReference *ref = static_cast<ModelReference*>(GetItemData(serverItem));
  ServerModel *serverModel = objectBrowserModel->FindServer(*ref);
  SubmitServerWork(serverModel, new RefreshDatabaseListWork(ref->GetServerId()));
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

void ObjectBrowser::LoadDatabase(const ObjectModelReference& databaseRef, IndexSchemaCompletionCallback *indexCompletion) {
  DatabaseModel *database = objectBrowserModel->FindDatabase(databaseRef);
  SubmitDatabaseWork(database, new LoadDatabaseSchemaWork(databaseRef, indexCompletion == NULL));
  SubmitDatabaseWork(database, new IndexDatabaseSchemaWork(databaseRef, indexCompletion));
  SubmitDatabaseWork(database, new LoadDatabaseDescriptionsWork(databaseRef));
}

void ObjectBrowser::LoadRelation(const ObjectModelReference& relationRef) {
  DatabaseModel *database = objectBrowserModel->FindDatabase(relationRef.DatabaseRef());
  RelationModel *relation = objectBrowserModel->FindRelation(relationRef);
  SubmitDatabaseWork(database, new LoadRelationWork(relation->type, relationRef));
}

void ObjectBrowser::UpdateServer(const wxString& serverId, bool expandAfter) {
  wxWindowUpdateLocker noUpdates(this);
  const ServerModel *serverModel = objectBrowserModel->FindServer(serverId);
  wxASSERT(serverModel != NULL);
  wxTreeItemId serverItem = FindServerItem(serverId);
  wxASSERT(serverItem.IsOk());

  wxString serverItemText = serverModel->Identification() + _T(" (") + serverModel->VersionString() + _T(")");
  if (serverModel->IsUsingSSL()) {
    serverItemText += _T(" [") + serverModel->GetSSLCipher() + _T("]");
    SetItemImage(serverItem, img_server_encrypted);
  }
  SetItemText(serverItem, serverItemText);

  const std::vector<DatabaseModel> &databases = serverModel->GetDatabases();
  std::vector<const DatabaseModel*> systemDatabases;
  std::vector<const DatabaseModel*> templateDatabases;
  std::vector<const DatabaseModel*> userDatabases;
  for (std::vector<DatabaseModel>::const_iterator iter = databases.begin(); iter != databases.end(); iter++) {
    const DatabaseModel& database = *iter;
    if (database.IsSystem()) {
      systemDatabases.push_back(&database);
    }
    else if (database.isTemplate) {
      templateDatabases.push_back(&database);
    }
    else {
      userDatabases.push_back(&database);
    }
  }

  const std::vector<TablespaceModel>& tablespaces = serverModel->GetTablespaces();
  std::vector<const TablespaceModel*> userTablespaces;
  std::vector<const TablespaceModel*> systemTablespaces;

  for (std::vector<TablespaceModel>::const_iterator iter = tablespaces.begin(); iter != tablespaces.end(); iter++) {
    if ((*iter).IsSystem())
      systemTablespaces.push_back(&(*iter));
    else
      userTablespaces.push_back(&(*iter));
  }

  if (!userDatabases.empty()) {
    AppendDatabaseItems(serverModel, serverItem, userDatabases);
  }

  if (!templateDatabases.empty()) {
    wxTreeItemId templateDatabasesItem = AppendItem(serverItem, _("Template Databases"));
    SetItemImage(templateDatabasesItem, img_folder);
    AppendDatabaseItems(serverModel, templateDatabasesItem, templateDatabases);
  }

  AppendRoleItems(serverModel, serverItem);

  if (!userTablespaces.empty()) {
    wxTreeItemId folderItem = AppendItem(serverItem, _("Tablespaces"));
    SetItemImage(folderItem, img_folder);
    AppendTablespaceItems(serverModel, folderItem, userTablespaces);
  }

  if (!systemDatabases.empty()) {
    wxTreeItemId systemDatabasesItem = AppendItem(serverItem, _("System Databases"));
    SetItemImage(systemDatabasesItem, img_folder);
    AppendDatabaseItems(serverModel, systemDatabasesItem, systemDatabases);
  }

  if (!systemTablespaces.empty()) {
    wxTreeItemId folderItem = AppendItem(serverItem, _("System Tablespaces"));
    SetItemImage(folderItem, img_folder);
    AppendTablespaceItems(serverModel, folderItem, systemTablespaces);
  }

  if (expandAfter) {
    EnsureVisible(serverItem);
    SelectItem(serverItem);
    Expand(serverItem);
  }
}

void ObjectBrowser::AppendDatabaseItems(const ServerModel *server, wxTreeItemId parentItem, const std::vector<const DatabaseModel*> &databases)
{
  for (std::vector<const DatabaseModel*>::const_iterator iter = databases.begin(); iter != databases.end(); iter++) {
    const DatabaseModel *database = *iter;
    wxTreeItemId databaseItem = AppendItem(parentItem, database->name);
    SetItemData(databaseItem, new ModelReference(server->Identification(), database->oid));
    SetItemImage(databaseItem, img_database);
    if (database->IsUsable())
      SetItemData(AppendItem(databaseItem, _T("Loading...")), new DatabaseLoader(this, *database));
    databaseItems[*database] = databaseItem;
  }
}

void ObjectBrowser::AppendTablespaceItems(const ServerModel *server, wxTreeItemId parentItem, const std::vector<const TablespaceModel*> &tablespaces)
{
  for (std::vector<const TablespaceModel*>::const_iterator iter = tablespaces.begin(); iter != tablespaces.end(); iter++) {
    const TablespaceModel *tablespace = *iter;
    wxTreeItemId spcItem = AppendItem(parentItem, tablespace->FormatName());
    SetItemData(spcItem, new ModelReference(server->Identification(), ObjectModelReference::PG_TABLESPACE, tablespace->oid));
    SetItemImage(spcItem, img_database);
  }
}

void ObjectBrowser::AppendRoleItems(const ServerModel *serverModel, wxTreeItemId serverItem)
{
  const std::vector<RoleModel>& roles = serverModel->GetRoles();
  wxTreeItemId usersItem = AppendItem(serverItem, _("Users"));
  SetItemImage(usersItem, img_folder);
  wxTreeItemId groupsItem = AppendItem(serverItem, _("Groups"));
  SetItemImage(groupsItem, img_folder);
  for (std::vector<RoleModel>::const_iterator iter = roles.begin(); iter != roles.end(); iter++) {
    const RoleModel& role = *iter;
    wxTreeItemId roleItem;
    if (role.canLogin) {
      roleItem = AppendItem(usersItem, role.name);
    }
    else {
      roleItem = AppendItem(groupsItem, role.name);
    }
    SetItemData(roleItem, new ModelReference(serverModel->Identification(), ObjectModelReference::PG_ROLE, role.oid));
    SetItemImage(roleItem, img_role);
  }
}

void ObjectBrowser::AppendSchemaMembers(const ObjectModelReference& databaseRef, wxTreeItemId parent, bool createSchemaItem, const wxString &schemaName, const std::vector<const SchemaMemberModel*> &members) {
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
  int textSearchDictionariesCount = 0;
  int textSearchParsersCount = 0;
  int textSearchTemplatesCount = 0;
  int textSearchConfigurationsCount = 0;
  for (std::vector<const SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
    const SchemaMemberModel *member = *iter;
    if (member->name.IsEmpty()) continue;
    const RelationModel *relation = dynamic_cast<const RelationModel*>(member);
    if (relation == NULL) {
      if ((dynamic_cast<const FunctionModel*>(member)) != NULL) ++functionsCount;
      else if ((dynamic_cast<const TextSearchDictionaryModel*>(member)) != NULL) ++textSearchDictionariesCount;
      else if ((dynamic_cast<const TextSearchParserModel*>(member)) != NULL) ++textSearchParsersCount;
      else if ((dynamic_cast<const TextSearchTemplateModel*>(member)) != NULL) ++textSearchTemplatesCount;
      else if ((dynamic_cast<const TextSearchConfigurationModel*>(member)) != NULL) ++textSearchConfigurationsCount;
      continue;
    }
    ++relationsCount;
    wxTreeItemId memberItem = AppendItem(parent, createSchemaItem ? relation->name : relation->schema + _T(".") + relation->name);
    SetItemData(memberItem, new ModelReference(databaseRef, ObjectModelReference::PG_CLASS, relation->oid));
    RegisterSymbolItem(databaseRef, relation->oid, memberItem);
    if (relation->type == RelationModel::TABLE || relation->type == RelationModel::VIEW)
      SetItemData(AppendItem(memberItem, _("Loading...")), new RelationLoader(this, databaseRef, relation->oid));
    switch (relation->type) {
    case RelationModel::TABLE:
      if (relation->unlogged)
        SetItemImage(memberItem, img_unlogged_table);
      else
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

  if (functionsCount > 0) {
    wxTreeItemId functionsItem = parent;
    if (relationsCount != 0 && createSchemaItem) {
      functionsItem = AppendItem(parent, _("Functions"));
      SetItemImage(functionsItem, img_folder);
    }
    else {
      // no relations, so add functions directly in the schema item
      functionsItem = parent;
    }

    for (std::vector<const SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
      const SchemaMemberModel *member = *iter;
      if (member->name.IsEmpty()) continue;
      const FunctionModel *function = dynamic_cast<const FunctionModel*>(member);
      if (function == NULL) continue;
      wxTreeItemId memberItem = AppendItem(functionsItem, createSchemaItem ? function->name + _T("(") + function->arguments + _T(")") : function->schema + _T(".") + function->name + _T("(") + function->arguments + _T(")"));
      SetItemData(memberItem, new ModelReference(databaseRef, ObjectModelReference::PG_PROC, function->oid));
      RegisterSymbolItem(databaseRef, function->oid, memberItem);
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

  if (textSearchDictionariesCount > 0) {
    wxTreeItemId dictionariesItem;
    if (relationsCount != 0 && createSchemaItem) {
      dictionariesItem = AppendItem(parent, _("Text Search Dictionaries"));
      SetItemImage(dictionariesItem, img_folder);
    }
    else {
      dictionariesItem = parent;
    }

    for (std::vector<const SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
      const SchemaMemberModel *member = *iter;
      if (member->name.empty()) continue;
      const TextSearchDictionaryModel *dict = dynamic_cast<const TextSearchDictionaryModel*>(member);
      if (dict == NULL) continue;
      wxTreeItemId memberItem = AppendItem(dictionariesItem, createSchemaItem ? dict->name : dict->schema + _T(".") + dict->name);
      SetItemData(memberItem, new ModelReference(databaseRef, ObjectModelReference::PG_TS_DICT, dict->oid));
      SetItemImage(memberItem, img_text_search_dictionary);
      RegisterSymbolItem(databaseRef, member->oid, memberItem);
    }
  }

  if (textSearchParsersCount > 0) {
    wxTreeItemId parsersItem;
    if (relationsCount != 0 && createSchemaItem) {
      parsersItem = AppendItem(parent, _("Text Search Parsers"));
      SetItemImage(parsersItem, img_folder);
    }
    else {
      parsersItem = parent;
    }

    for (std::vector<const SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
      const SchemaMemberModel *member = *iter;
      if (member->name.empty()) continue;
      const TextSearchParserModel *prs = dynamic_cast<const TextSearchParserModel*>(member);
      if (prs == NULL) continue;
      wxTreeItemId memberItem = AppendItem(parsersItem, createSchemaItem ? prs->name : prs->schema + _T(".") + prs->name);
      SetItemData(memberItem, new ModelReference(databaseRef, ObjectModelReference::PG_TS_PARSER, prs->oid));
      SetItemImage(memberItem, img_text_search_parser);
      RegisterSymbolItem(databaseRef, member->oid, memberItem);
    }
  }

  if (textSearchTemplatesCount > 0) {
    wxTreeItemId templatesItem;
    if (relationsCount != 0 && createSchemaItem) {
      templatesItem = AppendItem(parent, _("Text Search Templates"));
      SetItemImage(templatesItem, img_folder);
    }
    else {
      templatesItem = parent;
    }

    for (std::vector<const SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
      const SchemaMemberModel *member = *iter;
      if (member->name.empty()) continue;
      const TextSearchTemplateModel *tmpl = dynamic_cast<const TextSearchTemplateModel*>(member);
      if (tmpl == NULL) continue;
      wxTreeItemId memberItem = AppendItem(templatesItem, createSchemaItem ? tmpl->name : tmpl->schema + _T(".") + tmpl->name);
      SetItemData(memberItem, new ModelReference(databaseRef, ObjectModelReference::PG_TS_TEMPLATE, tmpl->oid));
      SetItemImage(memberItem, img_text_search_template);
      RegisterSymbolItem(databaseRef, member->oid, memberItem);
    }
  }

  if (textSearchConfigurationsCount > 0) {
    wxTreeItemId configurationsItem;
    if (relationsCount != 0 && createSchemaItem) {
      configurationsItem = AppendItem(parent, _("Text Search Configurations"));
      SetItemImage(configurationsItem, img_folder);
    }
    else {
      configurationsItem = parent;
    }

    for (std::vector<const SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
      const SchemaMemberModel *member = *iter;
      if (member->name.empty()) continue;
      const TextSearchConfigurationModel *cfg = dynamic_cast<const TextSearchConfigurationModel*>(member);
      if (cfg == NULL) continue;
      wxTreeItemId memberItem = AppendItem(configurationsItem, createSchemaItem ? cfg->name : cfg->schema + _T(".") + cfg->name);
      SetItemData(memberItem, new ModelReference(databaseRef, ObjectModelReference::PG_TS_CONFIG, cfg->oid));
      SetItemImage(memberItem, img_text_search_configuration);
      RegisterSymbolItem(databaseRef, member->oid, memberItem);
    }
  }
}

void ObjectBrowser::AppendDivision(const DatabaseModel *databaseModel, std::vector<const SchemaMemberModel*> &members, wxTreeItemId parentItem) {
  std::map<wxString, std::vector<const SchemaMemberModel*> > schemas;

  for (std::vector<const SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
    const SchemaMemberModel *member = *iter;
    schemas[member->schema].push_back(member);
  }

  bool foldSchemas = members.size() > 50 && schemas.size() > 1;
  for (std::map<wxString, std::vector<const SchemaMemberModel*> >::iterator iter = schemas.begin(); iter != schemas.end(); iter++) {
    AppendSchemaMembers(*databaseModel, parentItem, foldSchemas && iter->second.size() > 1, iter->first, iter->second);
  }
}

void ObjectBrowser::UpdateDatabase(const ObjectModelReference& databaseRef, bool expandAfter)
{
  const DatabaseModel *databaseModel = objectBrowserModel->FindDatabase(databaseRef);
  wxTreeItemId databaseItem = FindDatabaseItem(databaseRef);
  wxWindowUpdateLocker noUpdates(this);

  DatabaseModel::Divisions divisions = databaseModel->DivideSchemaMembers();

  AppendDivision(databaseModel, divisions.userDivision, databaseItem);

  if (!divisions.extensionDivisions.empty()) {
    wxTreeItemId extensionsItem = AppendItem(databaseItem, _("Extensions"));
    SetItemImage(extensionsItem, img_folder);
    for (std::map<wxString, std::vector<const SchemaMemberModel*> >::iterator iter = divisions.extensionDivisions.begin(); iter != divisions.extensionDivisions.end(); iter++) {
      wxTreeItemId extensionItem = AppendItem(extensionsItem, iter->first);
      SetItemImage(extensionItem, img_folder);
      AppendDivision(databaseModel, iter->second, extensionItem);
    }
  }

  wxTreeItemId systemDivisionItem = AppendItem(databaseItem, _("System schemas"));
  SetItemImage(systemDivisionItem, img_folder);
  wxTreeItemId systemDivisionLoaderItem = AppendItem(systemDivisionItem, _T("Loading..."));
  SetItemData(systemDivisionLoaderItem, new SystemSchemasLoader(this, *databaseModel, divisions.systemDivision));
  if (expandAfter) Expand(databaseItem);
  SetItemText(databaseItem, databaseModel->name); // remove loading message
}

void ObjectBrowser::UpdateRelation(const ObjectModelReference& relationRef)
{
  const RelationModel *relationModel = objectBrowserModel->FindRelation(relationRef);
  wxASSERT(relationModel != NULL);
  wxTreeItemId relationItem = FindRelationItem(relationRef);
  wxWindowUpdateLocker noUpdates(this);

  std::map<int,wxTreeItemId> columnItems;
  for (std::vector<ColumnModel>::const_iterator iter = relationModel->columns.begin(); iter != relationModel->columns.end(); iter++) {
    const ColumnModel& column = *iter;
    wxString itemText = column.name + _T(" (") + column.type;

    if (relationModel->type == RelationModel::TABLE) {
      if (column.nullable)
        itemText += _(", null");
      else
        itemText += _(", not null");
      if (column.hasDefault)
        itemText += _(", default");
    }

    itemText += _T(")");

    wxTreeItemId columnItem = AppendItem(relationItem, itemText);
    SetItemData(columnItem, new ModelReference(relationRef, ObjectModelReference::PG_ATTRIBUTE, relationModel->oid, column.attnum));
    SetItemImage(columnItem, img_column);
    columnItems[column.attnum] = columnItem;

    for (std::vector<RelationModel>::const_iterator seqIter = relationModel->sequences.begin(); seqIter != relationModel->sequences.end(); seqIter++) {
      const RelationModel& sequence = *seqIter;
      if (sequence.owningColumn != column.attnum) continue;

      wxTreeItemId sequenceItem = AppendItem(columnItem, sequence.schema + _T(".") + sequence.name);
      SetItemData(sequenceItem, new ModelReference(relationRef, ObjectModelReference::PG_CLASS, relationModel->oid));
      SetItemImage(sequenceItem, img_sequence);
    }
  }

  if (!relationModel->indices.empty()) {
    wxTreeItemId indicesItem = AppendItem(relationItem, _("Indices"));
    SetItemImage(indicesItem, img_folder);
    for (std::vector<IndexModel>::const_iterator iter = relationModel->indices.begin(); iter != relationModel->indices.end(); iter++) {
      wxTreeItemId indexItem = AppendItem(indicesItem, (*iter).name);
      SetItemData(indexItem, new ModelReference(relationRef, ObjectModelReference::PG_INDEX, (*iter).oid));
      if ((*iter).primaryKey)
        SetItemImage(indexItem, img_index_pkey);
      else if ((*iter).unique || (*iter).exclusion)
        SetItemImage(indexItem, img_index_uniq);
      else
        SetItemImage(indexItem, img_index);
      for (std::vector<IndexModel::Column>::const_iterator colIter = (*iter).columns.begin(); colIter != (*iter).columns.end(); colIter++) {
        wxTreeItemId indexColumnItem = AppendItem(indexItem, (*colIter).expression);
        if ((*colIter).column > 0) {
          SetItemImage(indexColumnItem, img_column);
          wxTreeItemId columnItem = columnItems[(*colIter).column];
          if ((*iter).primaryKey) {
            SetItemImage(columnItem, img_column_pkey);
          }
        }
        else {
          SetItemImage(indexColumnItem, img_function);
        }
      }
    }
  }

  if (!relationModel->checkConstraints.empty()) {
    wxTreeItemId constraintsItem = AppendItem(relationItem, _("Constraints"));
    SetItemImage(constraintsItem, img_folder);
    for (std::vector<CheckConstraintModel>::const_iterator iter = relationModel->checkConstraints.begin(); iter != relationModel->checkConstraints.end(); iter++) {
      wxTreeItemId constraintItem = AppendItem(constraintsItem, (*iter).name);
      SetItemData(constraintItem, new ModelReference(relationRef, ObjectModelReference::PG_CONSTRAINT, (*iter).oid));
    }
  }

  if (!relationModel->triggers.empty()) {
    wxTreeItemId triggersItem = AppendItem(relationItem, _("Triggers"));
    SetItemImage(triggersItem, img_folder);
    for (std::vector<TriggerModel>::const_iterator iter = relationModel->triggers.begin(); iter != relationModel->triggers.end(); iter++) {
      wxTreeItemId triggerItem = AppendItem(triggersItem, (*iter).name);
      SetItemData(triggerItem, new ModelReference(relationRef, ObjectModelReference::PG_TRIGGER, (*iter).oid));
    }
  }


  Expand(relationItem);

  // remove 'loading...' tag
  wxString itemText = GetItemText(relationItem);
  int space = itemText.Find(_T(' '));
  if (space != wxNOT_FOUND) {
    SetItemText(relationItem, itemText.Left(space));
  }
}

void ObjectBrowser::OnGetTooltip(wxTreeEvent &event)
{
  wxTreeItemId item = event.GetItem();
  wxString text = GetToolTipText(item);
  if (text.empty())
    event.Skip(true);
  else
    event.SetToolTip(text);
}

wxString ObjectBrowser::GetToolTipText(const wxTreeItemId& itemId) const
{
  wxTreeItemData *itemData = GetItemData(itemId);
  if (itemData == NULL) return wxEmptyString;
  ObjectModelReference *ref = dynamic_cast<ObjectModelReference*>(itemData);
  if (ref == NULL) return wxEmptyString;
  const ObjectModel *object = objectBrowserModel->FindObject(*ref);
  if (object == NULL) return wxEmptyString;
  if (object->description.empty())
    return object->FormatName();
  else
    return object->description;
}

bool ObjectBrowser::IsServerSelected() const {
  return !FindItemContext(GetSelection()).GetServerId().empty();
}

void ObjectBrowser::DisconnectSelected() {
  wxTreeItemId selected = GetSelection();
  if (!selected.IsOk()) {
    wxLogDebug(_T("selected item was not ok -- nothing is selected?"));
    return;
  }
  ModelReference ref = FindItemContext(selected);
  if (!ref.GetServerId().empty()) {
    wxLogDebug(_T("Disconnect: %s (menubar)"), ref.GetServerId().c_str());
    Delete(FindServerItem(ref.GetServerId()));
    objectBrowserModel->RemoveServer(ref.GetServerId());
    UpdateSelectedDatabase();
  }
}

class ZoomToFoundObjectOnCompletion : public ObjectFinder::Completion {
public:
  ZoomToFoundObjectOnCompletion(ObjectBrowser *ob, const ObjectModelReference& databaseRef) : databaseRef(databaseRef), ob(ob) {}
  void OnObjectChosen(const CatalogueIndex::Document *document) {
    ob->ZoomToFoundObject(databaseRef, document);
  }
  void OnCancelled() {
  }
private:
  ObjectModelReference databaseRef;
  ObjectBrowser *ob;
};

class OpenObjectFinderOnIndexSchemaCompletion : public IndexSchemaCompletionCallback {
protected:
  void Completed(ObjectBrowser *ob, const ObjectModelReference& databaseRef, const CatalogueIndex *catalogue) {
    ob->FindObject(databaseRef);
  }
};

void ObjectBrowser::FindObject(const ServerConnection &server, const wxString &dbname) {
  const DatabaseModel *database = objectBrowserModel->FindDatabase(server, dbname);
  wxASSERT(database != NULL);

  if (!database->loaded) {
    LoadDatabase(*database, new OpenObjectFinderOnIndexSchemaCompletion());
    return;
  }

  FindObject(*database);
}

void ObjectBrowser::FindObject(const ObjectModelReference& databaseRef) {
  const DatabaseModel *database = objectBrowserModel->FindDatabase(databaseRef);
  wxASSERT(database->loaded);
  wxASSERT(database->catalogueIndex != NULL);

  ObjectFinder *finder = new ObjectFinder(NULL, database->catalogueIndex);
  finder->SetFocus();
  Oid entityId = finder->ShowModal();
  if (entityId > 0) {
    ZoomToFoundObject(databaseRef, entityId);
  }
}

wxTreeItemId ObjectBrowser::FindServerItem(const wxString& serverId) const
{
  wxTreeItemIdValue cookie;
  wxTreeItemId childItem = GetFirstChild(GetRootItem(), cookie);
  do {
    wxASSERT(childItem.IsOk());
    ModelReference *ref = static_cast<ModelReference*>(GetItemData(childItem));
    if (ref->GetServerId() == serverId)
      return childItem;
    childItem = GetNextChild(GetRootItem(), cookie);
  } while (1);
}

wxTreeItemId ObjectBrowser::FindDatabaseItem(const ObjectModelReference& databaseRef) const
{
  std::map< ObjectModelReference, wxTreeItemId >::const_iterator itemPtr = databaseItems.find(databaseRef);
  wxASSERT(itemPtr != databaseItems.end());
  return (*itemPtr).second;
}

wxTreeItemId ObjectBrowser::FindSystemSchemasItem(const ObjectModelReference& database) const {
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

wxTreeItemId ObjectBrowser::FindRelationItem(const ObjectModelReference& databaseRef, Oid oid) const
{
  wxTreeItemId item = LookupSymbolItem(databaseRef, oid);
  wxASSERT(item.IsOk());
  return item;
}

wxTreeItemId ObjectBrowser::FindRelationItem(const ObjectModelReference& relationRef) const
{
  wxTreeItemId item = LookupSymbolItem(relationRef.DatabaseRef(), relationRef.GetOid());
  wxASSERT(item.IsOk());
  return item;
}

void ObjectBrowser::ZoomToFoundObject(const ObjectModelReference& databaseRef, Oid entityId) {
  wxTreeItemId item = LookupSymbolItem(databaseRef, entityId);
  if (!item.IsOk()) {
    item = FindSystemSchemasItem(databaseRef);
    LazyLoader *systemSchemasLoader = GetLazyLoader(item);
    if (systemSchemasLoader != NULL) {
      wxBusyCursor wait;
      wxLogDebug(_T("Forcing load of system schemas"));
      systemSchemasLoader->load(item);
      DeleteLazyLoader(item);
      item = LookupSymbolItem(databaseRef, entityId);
    }
  }
  wxASSERT(item.IsOk());
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

  ModelReference *ref = dynamic_cast<ModelReference*>(data);
  if (ref == NULL) return;

  switch (ref->GetObjectClass()) {
  case InvalidOid:
    contextMenuServer = objectBrowserModel->FindServer(*ref);
    PopupMenu(serverMenu);
    break;

  case ObjectModelReference::PG_DATABASE:
    contextMenuDatabase = objectBrowserModel->FindDatabase(*ref);
    PopupMenu(databaseMenu);
    break;

  case ObjectModelReference::PG_CLASS:
    {
      DatabaseModel *database = objectBrowserModel->FindDatabase(ref->DatabaseRef());
      RelationModel *relation = objectBrowserModel->FindRelation(*ref);
      contextMenuDatabase = database;
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
    }
    break;

  case ObjectModelReference::PG_PROC:
    {
      DatabaseModel *database = objectBrowserModel->FindDatabase(ref->DatabaseRef());
      FunctionModel *function = objectBrowserModel->FindFunction(*ref);
      contextMenuDatabase = database;
      contextMenuFunction = function;
      PopupMenu(functionMenu);
    }
    break;
  }
}

ObjectBrowser::ModelReference ObjectBrowser::FindItemContext(const wxTreeItemId &item) const
{
  for (wxTreeItemId cursor = item; cursor != GetRootItem(); cursor = GetItemParent(cursor)) {
    wxTreeItemData *data = GetItemData(cursor);
    if (data == NULL) continue;
    ModelReference *ref = dynamic_cast<ModelReference*>(data);
    if (ref == NULL) continue;
    return *ref;
  }

  return ModelReference(wxEmptyString);
}

void ObjectBrowser::OnItemSelected(wxTreeEvent &event)
{
  UpdateSelectedDatabase();
}

void ObjectBrowser::OnSetFocus(wxFocusEvent &event)
{
  event.Skip();
  UpdateSelectedDatabase();
}

void ObjectBrowser::UpdateSelectedDatabase() {
  wxTreeItemId selected = GetSelection();

  if (!selected.IsOk()) {
    wxCommandEvent event(PQWX_NoObjectSelected);
    ProcessEvent(event);
    return;
  }

  ModelReference ref = FindItemContext(selected);

  if (ref.GetServerId().empty()) {
    if (currentlySelected) {
      currentlySelected = false;
      selectedServer = NULL;
      selectedDatabase = NULL;
    }
    return;
  }

  ServerModel *server = objectBrowserModel->FindServer(ref.GetServerId());
  DatabaseModel *database;
  if (ref.GetObjectClass() == InvalidOid)
    database = NULL;
  else
    database = objectBrowserModel->FindDatabase(ref.DatabaseRef());

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
    wxString dbname;
    if (database) dbname = database->name;
    PQWXDatabaseEvent evt(server->conninfo, dbname, PQWX_ObjectSelected);
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
  objectBrowserModel->RemoveServer(contextMenuServer->Identification());
  Delete(contextMenuItem);
}

void ObjectBrowser::OnServerMenuRefresh(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Refresh server: ") + contextMenuServer->Identification());
}

void ObjectBrowser::OnServerMenuProperties(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Show server properties: ") + contextMenuServer->Identification());
}

void ObjectBrowser::OnDatabaseMenuQuery(wxCommandEvent &event)
{
  PQWXDatabaseEvent evt(contextMenuDatabase->server->conninfo, contextMenuDatabase->name, PQWX_ScriptNew);
  ProcessEvent(evt);
}

void ObjectBrowser::OnDatabaseMenuRefresh(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Refresh database: ") + contextMenuDatabase->server->Identification() + _T(" ") + contextMenuDatabase->name);
}

void ObjectBrowser::OnDatabaseMenuProperties(wxCommandEvent &event) {
  wxMessageBox(_T("TODO Show database properties: ") + contextMenuDatabase->server->Identification() + _T(" ") + contextMenuDatabase->name);
}

void ObjectBrowser::OnDatabaseMenuViewDependencies(wxCommandEvent &event) {
  DependenciesView *dialog = new DependenciesView(NULL, contextMenuDatabase->GetDatabaseConnection(), contextMenuDatabase->FormatName(), ObjectModelReference::PG_DATABASE, (Oid) contextMenuDatabase->oid, (Oid) contextMenuDatabase->oid);
  dialog->Show();
}

void ObjectBrowser::OnRelationMenuViewDependencies(wxCommandEvent &event) {
  DependenciesView *dialog = new DependenciesView(NULL, contextMenuDatabase->GetDatabaseConnection(), contextMenuRelation->FormatName(), ObjectModelReference::PG_CLASS, (Oid) contextMenuRelation->oid, (Oid) contextMenuDatabase->oid);
  dialog->Show();
}

void ObjectBrowser::OnFunctionMenuViewDependencies(wxCommandEvent &event) {
  DependenciesView *dialog = new DependenciesView(NULL, contextMenuDatabase->GetDatabaseConnection(), contextMenuRelation->FormatName(), ObjectModelReference::PG_PROC, (Oid) contextMenuFunction->oid, (Oid) contextMenuDatabase->oid);
  dialog->Show();
}

wxTreeItemId ObjectBrowser::LookupSymbolItem(const ObjectModelReference& database, Oid oid) const
{
  wxASSERT(database.GetObjectClass() == ObjectModelReference::PG_DATABASE);
  std::map< ObjectModelReference, std::map< Oid, wxTreeItemId > >::const_iterator tablePtr = symbolTables.find(database);
  wxASSERT(tablePtr != symbolTables.end());
  const std::map< Oid, wxTreeItemId >& symbolTable = (*tablePtr).second;

  std::map< Oid, wxTreeItemId >::const_iterator itemPtr = symbolTable.find(oid);
  if (itemPtr == symbolTable.end())
    return wxTreeItemId();
  else
    return (*itemPtr).second;
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
