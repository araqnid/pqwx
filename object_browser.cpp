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
#include "object_browser_work.h"
#include "object_browser_scripts.h"
#include "dependencies_view.h"
#include "static_resources.h"
#include "create_database_dialogue.h"
#include "script_events.h"
#include "ssl_info.h"

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

  EVT_MENU(XRCID("ServerMenu_NewDatabase"), ObjectBrowser::OnServerMenuNewDatabase)
  EVT_UPDATE_UI(XRCID("ServerMenu_NewDatabase"), ObjectBrowser::EnableIffHaveCreateDBPrivilege)
  EVT_MENU(XRCID("ServerMenu_Disconnect"), ObjectBrowser::OnServerMenuDisconnect)
  EVT_MENU(XRCID("ServerMenu_Properties"), ObjectBrowser::OnServerMenuProperties)
  EVT_MENU(XRCID("ServerMenu_Refresh"), ObjectBrowser::OnServerMenuRefresh)

  EVT_MENU(XRCID("DatabaseMenu_Query"), ObjectBrowser::OnDatabaseMenuQuery)
  EVT_UPDATE_UI(XRCID("DatabaseMenu_Query"), ObjectBrowser::EnableIffUsableDatabase)
  EVT_MENU(XRCID("DatabaseMenu_Drop"), ObjectBrowser::OnDatabaseMenuDrop)
  EVT_UPDATE_UI(XRCID("DatabaseMenu_Drop"), ObjectBrowser::EnableIffDroppableDatabase)
  EVT_MENU(XRCID("DatabaseMenu_Refresh"), ObjectBrowser::OnDatabaseMenuRefresh)
  EVT_UPDATE_UI(XRCID("DatabaseMenu_Refresh"), ObjectBrowser::EnableIffUsableDatabase)
  EVT_MENU(XRCID("DatabaseMenu_Properties"), ObjectBrowser::OnDatabaseMenuProperties)
  EVT_UPDATE_UI(XRCID("DatabaseMenu_Properties"), ObjectBrowser::EnableIffUsableDatabase)
  EVT_MENU(XRCID("DatabaseMenu_ViewDependencies"), ObjectBrowser::OnDatabaseMenuViewDependencies)
  EVT_UPDATE_UI(XRCID("DatabaseMenu_Script"), ObjectBrowser::EnableIffUsableDatabase)
  EVT_UPDATE_UI(XRCID("DatabaseMenu_ViewDependencies"), ObjectBrowser::EnableIffUsableDatabase)

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
  BIND_SCRIPT_HANDLERS(Index, Create)
  BIND_SCRIPT_HANDLERS(Index, Alter)
  BIND_SCRIPT_HANDLERS(Index, Drop)
  BIND_SCRIPT_HANDLERS(Schema, Create)
  BIND_SCRIPT_HANDLERS(Schema, Drop)
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
  BIND_SCRIPT_HANDLERS(TextSearchDictionary, Create)
  BIND_SCRIPT_HANDLERS(TextSearchDictionary, Alter)
  BIND_SCRIPT_HANDLERS(TextSearchDictionary, Drop)
  BIND_SCRIPT_HANDLERS(TextSearchParser, Create)
  BIND_SCRIPT_HANDLERS(TextSearchParser, Alter)
  BIND_SCRIPT_HANDLERS(TextSearchParser, Drop)
  BIND_SCRIPT_HANDLERS(TextSearchTemplate, Create)
  BIND_SCRIPT_HANDLERS(TextSearchTemplate, Alter)
  BIND_SCRIPT_HANDLERS(TextSearchTemplate, Drop)
  BIND_SCRIPT_HANDLERS(TextSearchConfiguration, Create)
  BIND_SCRIPT_HANDLERS(TextSearchConfiguration, Alter)
  BIND_SCRIPT_HANDLERS(TextSearchConfiguration, Drop)
  BIND_SCRIPT_HANDLERS(Tablespace, Create)
  BIND_SCRIPT_HANDLERS(Tablespace, Alter)
  BIND_SCRIPT_HANDLERS(Tablespace, Drop)
  BIND_SCRIPT_HANDLERS(Role, Create)
  BIND_SCRIPT_HANDLERS(Role, Alter)
  BIND_SCRIPT_HANDLERS(Role, Drop)
  BIND_SCRIPT_HANDLERS(Operator, Create)
  BIND_SCRIPT_HANDLERS(Operator, Alter)
  BIND_SCRIPT_HANDLERS(Operator, Drop)
  BIND_SCRIPT_HANDLERS(Type, Create)
  BIND_SCRIPT_HANDLERS(Type, Alter)
  BIND_SCRIPT_HANDLERS(Type, Drop)
END_EVENT_TABLE()

#define IMPLEMENT_SCRIPT_HANDLER(menu, mode, output, db, ref) \
void ObjectBrowser::On##menu##MenuScript##mode##output(wxCommandEvent &event) { \
  wxLogDebug(_T("Initiate %s script work for %s"), _T(#menu), ref.Identify().c_str()); \
  db->SubmitWork(new menu##ScriptWork(this, ref, ScriptWork::mode, ScriptWork::output)); \
}

#define IMPLEMENT_SCRIPT_HANDLERS(menu, mode, ref) \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, Window,    model.FindDatabase(ref.DatabaseRef()), ref) \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, File,      model.FindDatabase(ref.DatabaseRef()), ref) \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, Clipboard, model.FindDatabase(ref.DatabaseRef()), ref)

#define IMPLEMENT_SERVER_SCRIPT_HANDLERS(menu, mode, ref) \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, Window,    model.FindAdminDatabase(ref.ServerRef()), ref) \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, File,      model.FindAdminDatabase(ref.ServerRef()), ref) \
  IMPLEMENT_SCRIPT_HANDLER(menu, mode, Clipboard, model.FindAdminDatabase(ref.ServerRef()), ref)

IMPLEMENT_SCRIPT_HANDLERS(Database, Create, contextMenuRef.DatabaseRef())
IMPLEMENT_SCRIPT_HANDLERS(Database, Alter, contextMenuRef.DatabaseRef())
IMPLEMENT_SCRIPT_HANDLERS(Database, Drop, contextMenuRef.DatabaseRef())
IMPLEMENT_SCRIPT_HANDLERS(Table, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Table, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Table, Select, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Table, Insert, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Table, Update, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Table, Delete, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Index, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Index, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Index, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Schema, Create, FindContextSchema())
IMPLEMENT_SCRIPT_HANDLERS(Schema, Drop, FindContextSchema())
IMPLEMENT_SCRIPT_HANDLERS(View, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(View, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(View, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(View, Select, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Sequence, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Sequence, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Sequence, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Function, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Function, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Function, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Function, Select, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchDictionary, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchDictionary, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchDictionary, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchParser, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchParser, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchParser, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchTemplate, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchTemplate, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchTemplate, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchConfiguration, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchConfiguration, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(TextSearchConfiguration, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Operator, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Operator, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Operator, Drop, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Type, Create, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Type, Alter, contextMenuRef)
IMPLEMENT_SCRIPT_HANDLERS(Type, Drop, contextMenuRef)
IMPLEMENT_SERVER_SCRIPT_HANDLERS(Tablespace, Create, contextMenuRef)
IMPLEMENT_SERVER_SCRIPT_HANDLERS(Tablespace, Alter, contextMenuRef)
IMPLEMENT_SERVER_SCRIPT_HANDLERS(Tablespace, Drop, contextMenuRef)
IMPLEMENT_SERVER_SCRIPT_HANDLERS(Role, Create, contextMenuRef)
IMPLEMENT_SERVER_SCRIPT_HANDLERS(Role, Alter, contextMenuRef)
IMPLEMENT_SERVER_SCRIPT_HANDLERS(Role, Drop, contextMenuRef)

DEFINE_LOCAL_EVENT_TYPE(PQWX_ObjectSelected)
DEFINE_LOCAL_EVENT_TYPE(PQWX_NoObjectSelected)

class DatabaseLoader : public LazyLoader {
public:
  DatabaseLoader(ObjectBrowser& ob, const ObjectModelReference& databaseRef) : ob(ob), databaseRef(databaseRef) {}

  bool load(wxTreeItemId parent) {
    DatabaseModel *db = ob.Model().FindDatabase(databaseRef);
    if (!db->loaded) {
      db->Load();
      return true;
    }
    return false;
  }
  
private:
  ObjectBrowser& ob;
  const ObjectModelReference databaseRef;
};

class RelationLoader : public LazyLoader {
public:
  RelationLoader(ObjectBrowser& ob, const ObjectModelReference& databaseRef, Oid oid) : ob(ob), relationRef(databaseRef, ObjectModelReference::PG_CLASS, oid) {}

  bool load(wxTreeItemId parent)
  {
    DatabaseModel *database = ob.Model().FindDatabase(relationRef.DatabaseRef());
    wxASSERT(database != NULL);
    database->LoadRelation(relationRef);
    return true;
  }
  
private:
  ObjectBrowser& ob;
  const ObjectModelReference relationRef;
};

class SystemSchemasLoader : public LazyLoader {
public:
  SystemSchemasLoader(ObjectBrowser& ob, const ObjectModelReference& databaseRef, std::vector<const SchemaMemberModel*> division) : ob(ob), databaseRef(databaseRef), division(division) {}

  bool load(wxTreeItemId parent) {
    DatabaseModel *db = ob.Model().FindDatabase(databaseRef);
    wxWindowUpdateLocker noUpdates(&ob);
    ob.AppendDivision(db, division, false, parent);
    return false;
  }

private:
  ObjectBrowser& ob;
  const ObjectModelReference databaseRef;
  std::vector<const SchemaMemberModel*> division;
};

class AfterDatabaseCreated : public CreateDatabaseDialogue::ExecutionCallback {
public:
  AfterDatabaseCreated(ObjectModelReference server, ObjectBrowser& ob) : server(server), ob(ob) {}

  void ActionPerformed()
  {
    ServerModel *serverModel = ob.model.FindServer(server);
    wxASSERT(serverModel != NULL);
    serverModel->Load();
  }

private:
  const ObjectModelReference server;
  ObjectBrowser& ob;
};

ObjectBrowser::ObjectBrowser(ObjectBrowserModel& model, wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style) : wxTreeCtrl(parent, id, pos, size, style), model(model), contextMenuRef(wxEmptyString), selectedRef(wxEmptyString) {
  AddRoot(_T("root"));
  serverMenu = wxXmlResource::Get()->LoadMenu(_T("ServerMenu"));
  databaseMenu = wxXmlResource::Get()->LoadMenu(_T("DatabaseMenu"));
  tableMenu = wxXmlResource::Get()->LoadMenu(_T("TableMenu"));
  viewMenu = wxXmlResource::Get()->LoadMenu(_T("ViewMenu"));
  sequenceMenu = wxXmlResource::Get()->LoadMenu(_T("SequenceMenu"));
  functionMenu = wxXmlResource::Get()->LoadMenu(_T("FunctionMenu"));
  schemaMenu = wxXmlResource::Get()->LoadMenu(_T("SchemaMenu"));
  extensionMenu = wxXmlResource::Get()->LoadMenu(_T("ExtensionMenu"));
  textSearchParserMenu = wxXmlResource::Get()->LoadMenu(_T("TextSearchParserMenu"));
  textSearchTemplateMenu = wxXmlResource::Get()->LoadMenu(_T("TextSearchTemplateMenu"));
  textSearchDictionaryMenu = wxXmlResource::Get()->LoadMenu(_T("TextSearchDictionaryMenu"));
  textSearchConfigurationMenu = wxXmlResource::Get()->LoadMenu(_T("TextSearchConfigurationMenu"));
  indexMenu = wxXmlResource::Get()->LoadMenu(_T("IndexMenu"));
  roleMenu = wxXmlResource::Get()->LoadMenu(_T("RoleMenu"));
  tablespaceMenu = wxXmlResource::Get()->LoadMenu(_T("TablespaceMenu"));
  operatorMenu = wxXmlResource::Get()->LoadMenu(_T("OperatorMenu"));
  typeMenu = wxXmlResource::Get()->LoadMenu(_T("TypeMenu"));
  wxImageList *images = new wxImageList(13, 13, true);
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_folder.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_server.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_server_encrypted.png")));
  images->Add(StaticResources::LoadVFSImage(_T("memory:ObjectBrowser/icon_server_replica.png")));
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
  model.RegisterView(this);
}

void ObjectBrowser::AddServerConnection(const ServerConnection& server, DatabaseConnection *db) {
  ServerModel *serverModel = model.FindServer(server);
  if (serverModel != NULL) {
    wxLogDebug(_T("Ignoring server connection already registered in object browser: %s"), server.Identification().c_str());
    if (db != NULL) {
      db->CloseSync();
      delete db;
    }
    return;
  }

  serverModel = model.AddServerConnection(server, db);

  // setting the text twice is a bug workaround for wx 2.8
  // see http://trac.wxwidgets.org/ticket/10085
  wxLogDebug(_T("Connection added to object browser: %s"), server.Identification().c_str());
  wxTreeItemId serverItem = AppendItem(GetRootItem(), server.Identification());
  SetItemText(serverItem, server.Identification());
  SetItemData(serverItem, new ModelReference(server.Identification()));
  SetItemImage(serverItem, img_server);
  SetFocus();

  serverModel->Load();
}

void ObjectBrowser::Dispose()
{
  model.UnregisterView(this);
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

void ObjectBrowser::UpdateServer(const wxString& serverId, bool expandAfter) {
  wxWindowUpdateLocker noUpdates(this);
  const ServerModel *serverModel = model.FindServer(serverId);
  wxASSERT(serverModel != NULL);
  wxTreeItemId serverItem = FindServerItem(serverId);
  wxASSERT(serverItem.IsOk());

  wxString serverItemText = serverModel->Identification() + _T(" (") + serverModel->VersionString() + _T(")");
  if (serverModel->IsUsingSSL()) {
    const SSLInfo& sslInfo = serverModel->GetSSLInfo();
    if (!sslInfo.PeerDN().empty())
      serverItemText << _T(" ") << sslInfo.PeerDN();
    serverItemText << _T(" [") << sslInfo.Protocol()
                   << _T(":") << sslInfo.CipherBits()
                   << _T(":") << sslInfo.Cipher()
                   << _T("]");
    SetItemImage(serverItem, img_server_encrypted);
  }
  if (serverModel->GetReplicationState() != ServerModel::NOT_REPLICA) {
    SetItemImage(serverItem, img_server_replica);
  }
  SetItemText(serverItem, serverItemText);

  std::vector<ObjectModelReference> expanded;
  SaveExpandedObjects(serverItem, expanded);
  DeleteChildren(serverItem);

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

  if (expandAfter || !expanded.empty()) {
    RestoreExpandedObjects(serverItem, expanded);
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
      SetItemData(AppendItem(databaseItem, _T("Loading...")), new DatabaseLoader(*this, *database));
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

void ObjectBrowser::AppendEmptySchema(const DatabaseModel* db, wxTreeItemId parent, const SchemaModel& schema)
{
  wxString label = schema.name + _T(".");
  if (!schema.accessible) label << _(" (inaccessible)");
  wxTreeItemId emptySchemaItem = AppendItem(parent, label);
  SetItemData(emptySchemaItem, new ModelReference(*db, ObjectModelReference::PG_NAMESPACE, schema.oid));
  SetItemImage(emptySchemaItem, img_folder);
}

template<typename T>
wxString ObjectBrowser::FormatSchemaMemberItemName(const DatabaseModel* db, bool qualifyNames, const T& member)
{
  return qualifyNames ? member.QualifiedName() : member.UnqualifiedName();
}

wxString ObjectBrowser::FormatOperandType(const DatabaseModel* db, const OperatorModel& op, Oid typeOid)
{
  const TypeModel* type = static_cast<const TypeModel*>(db->FindObject(ObjectModelReference(*db, ObjectModelReference::PG_TYPE, typeOid)));
  if (type == NULL) {
    wxString unknown;
    unknown << typeOid << _T("?");
    return unknown;
  }
  else if (type->schema.name != _T("pg_catalog") && type->schema.name != op.schema.name)
    return type->QualifiedName();
  else
    return type->UnqualifiedName();
}

template<>
wxString ObjectBrowser::FormatSchemaMemberItemName(const DatabaseModel* db, bool qualifyNames, const OperatorModel& member)
{
  wxString opname;
  if (member.HasLeftOperand())
    opname << FormatOperandType(db, member, member.leftType) << _T(' ');
  if (qualifyNames)
    opname << _T("operator(") << member.QualifiedName() << _T(")");
  else
    opname << member.UnqualifiedName();
  if (member.HasRightOperand())
    opname << _T(' ') << FormatOperandType(db, member, member.rightType);
  return opname;
}

template<typename T>
void ObjectBrowser::AppendSchemaMemberItems(const DatabaseModel* db, wxTreeItemId parent, bool qualifyNames, bool fold, const wxString& title, Oid objectClass, int img, const std::vector<const SchemaMemberModel*> &members)
{
  wxTreeItemId membersItem;
  if (fold) {
    membersItem = AppendItem(parent, title);
    SetItemImage(membersItem, img_folder);
  }
  else {
    membersItem = parent;
  }

  for (std::vector<const SchemaMemberModel*>::const_iterator iter = members.begin(); iter != members.end(); iter++) {
    const SchemaMemberModel *member = *iter;
    if (member->name.empty()) continue;
    const T *obj = dynamic_cast<const T*>(member);
    if (obj == NULL) continue;
    wxTreeItemId memberItem = AppendItem(membersItem, FormatSchemaMemberItemName(db, qualifyNames, *obj));
    SetItemData(memberItem, new ModelReference(*db, objectClass, member->oid));
    SetItemImage(memberItem, img);
    RegisterSymbolItem(*db, member->oid, memberItem);
  }
}

void ObjectBrowser::AppendSchemaMembers(const DatabaseModel* db, wxTreeItemId parent, bool createSchemaItem, const SchemaModel& schema, const std::vector<const SchemaMemberModel*> &members) {
  if (createSchemaItem) {
    parent = AppendItem(parent, schema.name + _T("."));
    SetItemData(parent, new ModelReference(*db, ObjectModelReference::PG_NAMESPACE, schema.oid));
    SetItemImage(parent, img_folder);
  }

  int relationsCount = 0;
  int functionsCount = 0;
  int textSearchDictionariesCount = 0;
  int textSearchParsersCount = 0;
  int textSearchTemplatesCount = 0;
  int textSearchConfigurationsCount = 0;
  int typesCount = 0;
  int operatorsCount = 0;
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
      else if ((dynamic_cast<const TypeModel*>(member)) != NULL) ++typesCount;
      else if ((dynamic_cast<const OperatorModel*>(member)) != NULL) ++operatorsCount;
      continue;
    }
    ++relationsCount;
    wxTreeItemId memberItem = AppendItem(parent, createSchemaItem ? relation->UnqualifiedName() : relation->QualifiedName());
    SetItemData(memberItem, new ModelReference(*db, ObjectModelReference::PG_CLASS, relation->oid));
    RegisterSymbolItem(*db, relation->oid, memberItem);
    if (relation->type == RelationModel::TABLE || relation->type == RelationModel::VIEW)
      SetItemData(AppendItem(memberItem, _("Loading...")), new RelationLoader(*this, *db, relation->oid));
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
      wxTreeItemId memberItem = AppendItem(functionsItem, createSchemaItem ? function->UnqualifiedName() + _T("(") + function->arguments + _T(")") : function->QualifiedName() + _T("(") + function->arguments + _T(")"));
      SetItemData(memberItem, new ModelReference(*db, ObjectModelReference::PG_PROC, function->oid));
      RegisterSymbolItem(*db, function->oid, memberItem);
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
    AppendSchemaMemberItems<TextSearchDictionaryModel>(db, parent, !createSchemaItem, relationsCount != 0 && createSchemaItem, _("Text Search Dictionaries"), ObjectModelReference::PG_TS_DICT, img_text_search_dictionary, members);
  }

  if (textSearchParsersCount > 0) {
    AppendSchemaMemberItems<TextSearchParserModel>(db, parent, !createSchemaItem, relationsCount != 0 && createSchemaItem, _("Text Search Parsers"), ObjectModelReference::PG_TS_PARSER, img_text_search_parser, members);
  }

  if (textSearchTemplatesCount > 0) {
    AppendSchemaMemberItems<TextSearchTemplateModel>(db, parent, !createSchemaItem, relationsCount != 0 && createSchemaItem, _("Text Search Templates"), ObjectModelReference::PG_TS_TEMPLATE, img_text_search_template, members);
  }

  if (textSearchConfigurationsCount > 0) {
    AppendSchemaMemberItems<TextSearchConfigurationModel>(db, parent, !createSchemaItem, relationsCount != 0 && createSchemaItem, _("Text Search Configurations"), ObjectModelReference::PG_TS_CONFIG, img_text_search_configuration, members);
  }

  if (typesCount > 0) {
    AppendSchemaMemberItems<TypeModel>(db, parent, !createSchemaItem, relationsCount != 0 && createSchemaItem, _("Types"), ObjectModelReference::PG_TYPE, img_folder, members);
  }

  if (operatorsCount > 0) {
    AppendSchemaMemberItems<OperatorModel>(db, parent, !createSchemaItem, relationsCount != 0 && createSchemaItem, _("Operators"), ObjectModelReference::PG_OPERATOR, img_folder, members);
  }
}

void ObjectBrowser::AppendDivision(const DatabaseModel *db, std::vector<const SchemaMemberModel*> &members, bool includeEmptySchemas, wxTreeItemId parentItem) {
  std::map<Oid, std::vector<const SchemaMemberModel*> > schemaMembers;
  const ObjectModelReference databaseRef = *db;

  for (std::vector<const SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
    const SchemaMemberModel *member = *iter;
    schemaMembers[member->schema.oid].push_back(member);
  }

  bool foldSchemas = members.size() > 50 && schemaMembers.size() > 1;
  for (std::vector<SchemaModel>::const_iterator iter = db->schemas.begin(); iter != db->schemas.end(); iter++) {
    const SchemaModel& schema = *iter;
    std::map<Oid, std::vector<const SchemaMemberModel*> >::const_iterator membersPtr = schemaMembers.find(schema.oid);
    if (membersPtr != schemaMembers.end()) {
      const std::vector<const SchemaMemberModel*>& thisSchemaMembers = membersPtr->second;
      AppendSchemaMembers(db, parentItem, foldSchemas && thisSchemaMembers.size() > 1, schema, thisSchemaMembers);
    }
    else if (includeEmptySchemas) {
      if (!schema.IsSystem()) // bodge
        AppendEmptySchema(db, parentItem, schema);
    }
  }
}

void ObjectBrowser::UpdateDatabase(const ObjectModelReference& databaseRef, bool expandAfter)
{
  const DatabaseModel *databaseModel = model.FindDatabase(databaseRef);
  wxTreeItemId databaseItem = FindDatabaseItem(databaseRef);
  wxWindowUpdateLocker noUpdates(this);

  std::vector<ObjectModelReference> expanded;
  SaveExpandedObjects(databaseItem, expanded);
  DeleteChildren(databaseItem);

  DatabaseModel::Divisions divisions = databaseModel->DivideSchemaMembers();

  AppendDivision(databaseModel, divisions.userDivision, true, databaseItem);

  if (!divisions.extensionDivisions.empty()) {
    wxTreeItemId extensionsItem = AppendItem(databaseItem, _("Extensions"));
    SetItemImage(extensionsItem, img_folder);
    for (std::map<const ExtensionModel, std::vector<const SchemaMemberModel*> >::iterator iter = divisions.extensionDivisions.begin(); iter != divisions.extensionDivisions.end(); iter++) {
      wxTreeItemId extensionItem = AppendItem(extensionsItem, iter->first.name);
      SetItemImage(extensionItem, img_folder);
      SetItemData(extensionItem, new ModelReference(databaseRef, ObjectModelReference::PG_EXTENSION, iter->first.oid));
      AppendDivision(databaseModel, iter->second, false, extensionItem);
    }
  }

  wxTreeItemId systemDivisionItem = AppendItem(databaseItem, _("System schemas"));
  SetItemImage(systemDivisionItem, img_folder);
  wxTreeItemId systemDivisionLoaderItem = AppendItem(systemDivisionItem, _T("Loading..."));
  SetItemData(systemDivisionLoaderItem, new SystemSchemasLoader(*this, *databaseModel, divisions.systemDivision));
  if (expandAfter || !expanded.empty()) {
    Expand(databaseItem);
    RestoreExpandedObjects(databaseItem, expanded);
  }
  SetItemText(databaseItem, databaseModel->name); // remove loading message
}

void ObjectBrowser::UpdateRelation(const ObjectModelReference& relationRef)
{
  const RelationModel *relationModel = model.FindRelation(relationRef);
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
    SetItemData(columnItem, new ModelReference(relationRef.DatabaseRef(), ObjectModelReference::PG_ATTRIBUTE, relationModel->oid, column.attnum));
    SetItemImage(columnItem, img_column);
    columnItems[column.attnum] = columnItem;

    for (std::vector<RelationModel>::const_iterator seqIter = relationModel->sequences.begin(); seqIter != relationModel->sequences.end(); seqIter++) {
      const RelationModel& sequence = *seqIter;
      if (sequence.owningColumn != column.attnum) continue;

      wxString sequenceName;
      if (sequence.schema.name == _T("pg_catalog") || sequence.schema.name == relationModel->schema.name)
        sequenceName = sequence.UnqualifiedName();
      else
        sequenceName = sequence.QualifiedName();
      wxTreeItemId sequenceItem = AppendItem(columnItem, sequenceName);
      SetItemData(sequenceItem, new ModelReference(relationRef.DatabaseRef(), ObjectModelReference::PG_CLASS, relationModel->oid));
      SetItemImage(sequenceItem, img_sequence);
    }
  }

  if (!relationModel->indices.empty()) {
    wxTreeItemId indicesItem = AppendItem(relationItem, _("Indices"));
    SetItemImage(indicesItem, img_folder);
    for (std::vector<IndexModel>::const_iterator iter = relationModel->indices.begin(); iter != relationModel->indices.end(); iter++) {
      wxTreeItemId indexItem = AppendItem(indicesItem, (*iter).name);
      SetItemData(indexItem, new ModelReference(relationRef.DatabaseRef(), ObjectModelReference::PG_INDEX, (*iter).oid));
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
  const ObjectModel *object = model.FindObject(*ref);
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
    model.RemoveServer(ref.GetServerId());
    UpdateSelectedDatabase();
  }
}

class ZoomToFoundObjectOnCompletion : public ObjectFinder::Completion {
public:
  ZoomToFoundObjectOnCompletion(ObjectBrowser& ob, const ObjectModelReference& databaseRef) : databaseRef(databaseRef), ob(ob) {}
  void OnObjectChosen(const CatalogueIndex::Document *document) {
    ob.ZoomToFoundObject(databaseRef, document);
  }
  void OnCancelled() {
  }
private:
  ObjectModelReference databaseRef;
  ObjectBrowser& ob;
};

class OpenObjectFinderOnIndexSchemaCompletion : public IndexSchemaCompletionCallback {
public:
  OpenObjectFinderOnIndexSchemaCompletion(ObjectBrowser& ob, const ObjectModelReference& databaseRef) : ob(ob), databaseRef(databaseRef) {}
  void Completed(const CatalogueIndex& catalogue)
  {
    wxEndBusyCursor();
    ob.FindObject(databaseRef);
  }
  void Crashed()
  {
    wxEndBusyCursor();
  }
private:
  ObjectBrowser& ob;
  const ObjectModelReference databaseRef;
};

void ObjectBrowser::FindObject(const ServerConnection &server, const wxString &dbname) {
  DatabaseModel *database = model.FindDatabase(server, dbname);
  wxASSERT(database != NULL);

  if (!database->loaded) {
    wxBeginBusyCursor();
    database->Load(new OpenObjectFinderOnIndexSchemaCompletion(*this, *database));
    return;
  }

  FindObject(*database);
}

void ObjectBrowser::FindObject(const ObjectModelReference& databaseRef) {
  const DatabaseModel *database = model.FindDatabase(databaseRef);
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

const ServerModel* ObjectBrowser::ContextMenuServer()
{
  ModelReference *ref = dynamic_cast<ModelReference*>(GetItemData(contextMenuItem));
  if (ref == NULL) {
    // no object relevant
    return NULL;
  }
  return model.FindServer(ref->ServerRef());
}

void ObjectBrowser::EnableIffHaveCreateDBPrivilege(wxUpdateUIEvent& event)
{
  const ServerModel* server = ContextMenuServer();
  event.Enable(server != NULL && (server->HaveCreateDBPrivilege() || server->HaveSuperuserStatus()));
}

const DatabaseModel* ObjectBrowser::ContextMenuDatabase()
{
  ModelReference *ref = dynamic_cast<ModelReference*>(GetItemData(contextMenuItem));
  if (ref == NULL) {
    // no object relevant
    return NULL;
  }
  if (ref->GetObjectClass() == InvalidOid) {
    // server
    return NULL;
  }
  return model.FindDatabase(ref->DatabaseRef());
}

void ObjectBrowser::EnableIffDroppableDatabase(wxUpdateUIEvent& event)
{
  const DatabaseModel* database = ContextMenuDatabase();
  const ServerModel* server = ContextMenuServer();
  event.Enable(database != NULL && !database->IsSystem() && (server->HaveSuperuserStatus() || database->owner == server->GetRolename()));
}

void ObjectBrowser::EnableIffUsableDatabase(wxUpdateUIEvent& event)
{
  const DatabaseModel* database = ContextMenuDatabase();
  event.Enable(database != NULL && database->IsUsable());
}

void ObjectBrowser::PrepareServerMenu(const ServerModel* server)
{
}

void ObjectBrowser::PrepareDatabaseMenu(const DatabaseModel* database)
{
}

void ObjectBrowser::PrepareSchemaMenu(const SchemaModel* database)
{
}

void ObjectBrowser::OpenServerMemberMenu(wxMenu *menu, int serverItemId, const ServerMemberModel *member, const ServerModel *server)
{
  PrepareServerMenu(server);
  PopupMenu(menu);
}

void ObjectBrowser::OpenDatabaseMemberMenu(wxMenu *menu, int databaseItemId, const DatabaseMemberModel *member, const DatabaseModel *database)
{
  wxMenuItem *databaseItem = menu->FindItem(databaseItemId, NULL);
  wxASSERT(databaseItem != NULL);
  databaseItem->SetItemLabel(wxString::Format(_("Database '%s'"), database->name.c_str()));
  PrepareDatabaseMenu(database);
  OpenServerMemberMenu(menu, XRCID("DatabaseMenu_Server"), database, database->server);
}

void ObjectBrowser::OpenSchemaMemberMenu(wxMenu *menu, int schemaItemId, const SchemaMemberModel *member, const DatabaseModel *database)
{
  wxMenuItem *schemaItem = menu->FindItem(schemaItemId, NULL);
  wxASSERT(schemaItem != NULL);
  schemaItem->SetItemLabel(wxString::Format(_("Schema '%s'"), member->schema.name.c_str()));
  PrepareSchemaMenu(&(member->schema));
  OpenDatabaseMemberMenu(menu, XRCID("SchemaMenu_Database"), &member->schema, database);
}

void ObjectBrowser::OnItemRightClick(wxTreeEvent &event)
{
  contextMenuItem = event.GetItem();

  wxTreeItemData *data = GetItemData(contextMenuItem);
  if (data == NULL) {
    // some day this will never happen, when every tree node has something useful
    return;
  }

  ModelReference *ref = dynamic_cast<ModelReference*>(data);
  if (ref == NULL) return;

  contextMenuRef = *ref;

  switch (ref->GetObjectClass()) {
  case InvalidOid:
    PrepareServerMenu(model.FindServer(*ref));
    PopupMenu(serverMenu);
    break;

  case ObjectModelReference::PG_DATABASE:
    PrepareDatabaseMenu(model.FindDatabase(*ref));
    OpenServerMemberMenu(databaseMenu, XRCID("DatabaseMenu_Server"), model.FindDatabase(*ref), model.FindServer(ref->GetServerId()));
    break;

  case ObjectModelReference::PG_ROLE:
    OpenServerMemberMenu(roleMenu, XRCID("RoleMenu_Server"), static_cast<RoleModel*>(model.FindObject(*ref)), model.FindServer(ref->GetServerId()));
    break;

  case ObjectModelReference::PG_TABLESPACE:
    OpenServerMemberMenu(tablespaceMenu, XRCID("TablespaceMenu_Server"), static_cast<TablespaceModel*>(model.FindObject(*ref)), model.FindServer(ref->GetServerId()));
    break;

  case ObjectModelReference::PG_CLASS:
    {
      const RelationModel *relation = model.FindRelation(*ref);
      const DatabaseModel *database = model.FindDatabase(ref->DatabaseRef());
      switch (relation->type) {
      case RelationModel::TABLE:
        OpenSchemaMemberMenu(tableMenu, XRCID("TableMenu_Schema"), relation, database);
        break;
      case RelationModel::VIEW:
        OpenSchemaMemberMenu(viewMenu, XRCID("ViewMenu_Schema"), relation, database);
        break;
      case RelationModel::SEQUENCE:
        OpenSchemaMemberMenu(sequenceMenu, XRCID("SequenceMenu_Schema"), relation, database);
        break;
      }
    }
    break;

  case ObjectModelReference::PG_PROC:
    OpenSchemaMemberMenu(functionMenu, XRCID("FunctionMenu_Schema"), model.FindFunction(*ref), model.FindDatabase(ref->DatabaseRef()));
    break;

  case ObjectModelReference::PG_EXTENSION:
    OpenDatabaseMemberMenu(extensionMenu, XRCID("ExtensionMenu_Database"), static_cast<const ExtensionModel*>(model.FindObject(*ref)), model.FindDatabase(ref->DatabaseRef()));
    break;

  case ObjectModelReference::PG_NAMESPACE:
    PrepareSchemaMenu(static_cast<const SchemaModel*>(model.FindObject(*ref)));
    OpenDatabaseMemberMenu(schemaMenu, XRCID("SchemaMenu_Database"), static_cast<const SchemaModel*>(model.FindObject(*ref)), model.FindDatabase(ref->DatabaseRef()));
    break;

  case ObjectModelReference::PG_TS_PARSER:
    OpenSchemaMemberMenu(textSearchParserMenu, XRCID("TextSearchParserMenu_Schema"), static_cast<const TextSearchParserModel*>(model.FindObject(*ref)), model.FindDatabase(ref->DatabaseRef()));
    break;

  case ObjectModelReference::PG_TS_DICT:
    OpenSchemaMemberMenu(textSearchDictionaryMenu, XRCID("TextSearchDictionaryMenu_Schema"), static_cast<const TextSearchDictionaryModel*>(model.FindObject(*ref)), model.FindDatabase(ref->DatabaseRef()));
    break;

  case ObjectModelReference::PG_TS_TEMPLATE:
    OpenSchemaMemberMenu(textSearchTemplateMenu, XRCID("TextSearchTemplateMenu_Schema"), static_cast<const TextSearchTemplateModel*>(model.FindObject(*ref)), model.FindDatabase(ref->DatabaseRef()));
    break;

  case ObjectModelReference::PG_TS_CONFIG:
    OpenSchemaMemberMenu(textSearchConfigurationMenu, XRCID("TextSearchConfigurationMenu_Schema"), static_cast<const TextSearchConfigurationModel*>(model.FindObject(*ref)), model.FindDatabase(ref->DatabaseRef()));
    break;

  case ObjectModelReference::PG_OPERATOR:
    OpenSchemaMemberMenu(operatorMenu, XRCID("OperatorMenu_Schema"), static_cast<const OperatorModel*>(model.FindObject(*ref)), model.FindDatabase(ref->DatabaseRef()));
    break;

  case ObjectModelReference::PG_TYPE:
    OpenSchemaMemberMenu(typeMenu, XRCID("TypeMenu_Schema"), static_cast<const TypeModel*>(model.FindObject(*ref)), model.FindDatabase(ref->DatabaseRef()));
    break;

  case ObjectModelReference::PG_INDEX: {
    RelationModel *relation = NULL;
    for (wxTreeItemId cursor = contextMenuItem; cursor != GetRootItem(); cursor = GetItemParent(cursor)) {
      wxTreeItemData *data = GetItemData(cursor);
      if (data == NULL) continue;
      ModelReference *ref = dynamic_cast<ModelReference*>(data);
      if (ref == NULL) return;
      wxLogDebug(_T("Ancestor: %s"), ref->Identify().c_str());
      if (ref->GetObjectClass() == ObjectModelReference::PG_CLASS) {
        relation = model.FindRelation(*ref);
        break;
      }
    }
    wxASSERT(relation != NULL);
    OpenSchemaMemberMenu(indexMenu, XRCID("IndexMenu_Schema"), relation, model.FindDatabase(ref->DatabaseRef()));
    break;
  }

  default:
    wxLogDebug(_T("%s: No context menu applicable"), ref->Identify().c_str());
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

void ObjectBrowser::UpdateSelectedDatabase()
{
  wxTreeItemId selected = GetSelection();

  if (!selected.IsOk()) {
    if (!IsObjectSelected())
      return;
    UnmarkSelected();
    wxCommandEvent event(PQWX_NoObjectSelected);
    ProcessEvent(event);
    return;
  }

  ModelReference ref = FindItemContext(selected);

  if (ref.GetServerId().empty()) {
    UnmarkSelected();
    wxCommandEvent event(PQWX_NoObjectSelected);
    ProcessEvent(event);
    return;
  }

  if (ref != selectedRef) {
    DatabaseModel *database = model.FindDatabase(ref.DatabaseRef());
    ServerModel *server = model.FindServer(ref.ServerRef());
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
    selectedRef = ref;
  }
}

void ObjectBrowser::OnServerMenuNewDatabase(wxCommandEvent &event)
{
  const ServerModel *server = model.FindServer(contextMenuRef.GetServerId());
  wxASSERT(server != NULL);
  CreateDatabaseDialogue *dbox = new CreateDatabaseDialogue(this, new ServerWorkLauncher(*this, server->GetServerAdminDatabaseRef()), new AfterDatabaseCreated(contextMenuRef, *this));
  dbox->Show();
}

void ObjectBrowser::OnServerMenuDisconnect(wxCommandEvent &event) {
  const ServerModel *server = model.FindServer(contextMenuRef.GetServerId());
  wxASSERT(server != NULL);
  wxLogDebug(_T("Disconnect: %s (context menu)"), server->Identification().c_str());
  model.RemoveServer(server->Identification());
  Delete(contextMenuItem);
}

void ObjectBrowser::OnServerMenuRefresh(wxCommandEvent &event) {
  ServerModel *server = model.FindServer(contextMenuRef);
  wxASSERT(server != NULL);
  server->Load();
}

void ObjectBrowser::OnServerMenuProperties(wxCommandEvent &event) {
  const ServerModel *server = model.FindServer(contextMenuRef.GetServerId());
  wxASSERT(server != NULL);
  wxMessageBox(_T("TODO Show server properties: ") + server->Identification());
}

void ObjectBrowser::OnDatabaseMenuQuery(wxCommandEvent &event)
{
  const DatabaseModel *database = model.FindDatabase(contextMenuRef);
  wxASSERT(database != NULL);
  PQWXDatabaseEvent evt(database->server->conninfo, database->name, PQWX_ScriptNew);
  ProcessEvent(evt);
}

void ObjectBrowser::OnDatabaseMenuDrop(wxCommandEvent &event)
{
  DatabaseModel *database = model.FindDatabase(contextMenuRef);
  wxASSERT(database != NULL);
  database->Drop();
}

void ObjectBrowser::OnDatabaseMenuRefresh(wxCommandEvent &event) {
  DatabaseModel *database = model.FindDatabase(contextMenuRef);
  wxASSERT(database != NULL);
  database->Load();
}

void ObjectBrowser::OnDatabaseMenuProperties(wxCommandEvent &event) {
  const DatabaseModel *database = model.FindDatabase(contextMenuRef);
  wxASSERT(database != NULL);
  wxMessageBox(_T("TODO Show database properties: ") + database->server->Identification() + _T(" ") + database->name);
}

void ObjectBrowser::OnDatabaseMenuViewDependencies(wxCommandEvent &event) {
  DatabaseModel *database = model.FindDatabase(contextMenuRef);
  wxASSERT(database != NULL);
  DependenciesView *dialog = new DependenciesView(NULL, new DatabaseWorkLauncher(*this, contextMenuRef), database->FormatName(), contextMenuRef);
  dialog->Show();
}

void ObjectBrowser::OnRelationMenuViewDependencies(wxCommandEvent &event) {
  RelationModel *relation = model.FindRelation(contextMenuRef);
  wxASSERT(relation != NULL);
  DependenciesView *dialog = new DependenciesView(NULL, new DatabaseWorkLauncher(*this, contextMenuRef.DatabaseRef()), relation->FormatName(), contextMenuRef);
  dialog->Show();
}

void ObjectBrowser::OnFunctionMenuViewDependencies(wxCommandEvent &event) {
  FunctionModel *function = model.FindFunction(contextMenuRef);
  wxASSERT(function != NULL);
  DependenciesView *dialog = new DependenciesView(NULL, new DatabaseWorkLauncher(*this, contextMenuRef.DatabaseRef()), function->FormatName(), contextMenuRef);
  dialog->Show();
}

wxTreeItemId ObjectBrowser::LookupSymbolItem(const ObjectModelReference& database, Oid oid) const
{
  wxASSERT(database.GetObjectClass() == ObjectModelReference::PG_DATABASE);
  std::map< ObjectModelReference, std::map< Oid, wxTreeItemId > >::const_iterator tablePtr = symbolTables.find(database);
  if (tablePtr == symbolTables.end())
    return wxTreeItemId();

  const std::map< Oid, wxTreeItemId >& symbolTable = (*tablePtr).second;

  std::map< Oid, wxTreeItemId >::const_iterator itemPtr = symbolTable.find(oid);
  if (itemPtr == symbolTable.end())
    return wxTreeItemId();
  else
    return (*itemPtr).second;
}

ObjectModelReference ObjectBrowser::FindContextSchema()
{
  if (contextMenuRef.GetObjectClass() == ObjectModelReference::PG_NAMESPACE)
    return contextMenuRef;
  const ObjectModel *object = model.FindObject(contextMenuRef);
  wxASSERT(object != NULL);
  const SchemaMemberModel *schemaMember = dynamic_cast<const SchemaMemberModel*>(object);
  wxASSERT(schemaMember != NULL);
  return ObjectModelReference(contextMenuRef.DatabaseRef(), ObjectModelReference::PG_NAMESPACE, schemaMember->schema.oid);
}

void ObjectBrowser::SaveExpandedObjects(wxTreeItemId item, std::vector<ObjectModelReference>& output)
{
  if (!IsExpanded(item))
    return;

  wxTreeItemData *data = GetItemData(item);
  if (data != NULL) {
    ObjectModelReference *ref = dynamic_cast<ObjectModelReference*>(data);
    if (ref != NULL) {
      output.push_back(*ref);
    }
  }

  wxTreeItemIdValue cookie;
  for (wxTreeItemId child = GetFirstChild(item, cookie); child.IsOk(); child = GetNextChild(item, cookie)) {
    SaveExpandedObjects(child, output);
  }
}

void ObjectBrowser::RestoreExpandedObjects(wxTreeItemId item, const std::vector<ObjectModelReference>& saved)
{
  wxTreeItemData *data = GetItemData(item);
  if (data != NULL) {
    ObjectModelReference *ref = dynamic_cast<ObjectModelReference*>(data);
    if (ref != NULL) {
      if (std::find(saved.begin(), saved.end(), *ref) != saved.end()) {
        for (wxTreeItemId cursor = item; cursor.IsOk(); cursor = GetItemParent(cursor)) {
          if (IsExpanded(cursor))
            break;
          Expand(cursor);
        }
      }
    }
  }

  wxTreeItemIdValue cookie;
  for (wxTreeItemId child = GetFirstChild(item, cookie); child.IsOk(); child = GetNextChild(item, cookie)) {
    RestoreExpandedObjects(child, saved);
  }
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
