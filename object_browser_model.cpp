#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser.h"
#include "object_browser_model.h"
#include "object_browser_database_work.h"

BEGIN_EVENT_TABLE(ObjectBrowserModel, wxEvtHandler)
  PQWX_OBJECT_BROWSER_WORK_FINISHED(wxID_ANY, ObjectBrowserModel::OnWorkFinished)
  PQWX_OBJECT_BROWSER_WORK_CRASHED(wxID_ANY, ObjectBrowserModel::OnWorkCrashed)
END_EVENT_TABLE()

void ObjectBrowserModel::OnWorkFinished(wxCommandEvent &e) {
  ObjectBrowserWork *work = static_cast<ObjectBrowserWork*>(e.GetClientData());

  wxLogDebug(_T("%p: work finished (received by model)"), work);
  work->UpdateModel(this);

  delete work;
}

void ObjectBrowserModel::OnWorkCrashed(wxCommandEvent &e)
{
  ObjectBrowserWork *work = static_cast<ObjectBrowserWork*>(e.GetClientData());

  wxLogDebug(_T("%p: work crashed (received by model)"), work);
  if (!work->GetCrashMessage().empty()) {
    wxLogError(_T("%s\n%s"), _("An unexpected error occurred interacting with the database. Failure will ensue."), work->GetCrashMessage().c_str());
  }
  else {
    wxLogError(_T("%s"), _("An unexpected and unidentified error occurred interacting with the database. Failure will ensue."));
  }

  delete work;
}

wxString ObjectModelReference::Identify() const
{
  wxString buf;
  buf << serverId;
  if (database != InvalidOid) {
    buf << _T("|database=") << database;
    if (regclass != PG_DATABASE) {
      buf << _T("|regclass=") << regclass << _T("|oid=") << oid;
      if (subid)
        buf << _T("|objsubid=") << subid;
    }
  }
  return buf;
}

ServerModel *ObjectBrowserModel::FindServerById(const wxString &serverId)
{
  for (std::list<ServerModel>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    if ((*iter).Identification() == serverId) {
      return &(*iter);
    }
  }

  return NULL;
}

ObjectModel* ObjectBrowserModel::FindObject(const ObjectModelReference& ref)
{
  ServerModel *server = FindServerById(ref.GetServerId());
  if (ref.GetObjectClass() == InvalidOid)
    return server;
  else
    return server->FindObject(ref);
}

ServerModel* ObjectBrowserModel::AddServerConnection(const ServerConnection &server, DatabaseConnection *db)
{
  if (db) {
    servers.push_back(ServerModel(server, db));
    if (db->IsConnected()) {
      SetupDatabaseConnection(db);
    }
  }
  else {
    servers.push_back(ServerModel(server));
  }

  return &(servers.back());
}

void ObjectBrowserModel::SetupDatabaseConnection(DatabaseConnection *db)
{
  db->AddWork(new ObjectBrowserDatabaseWork(this, new SetupDatabaseConnectionWork()));
}

void ObjectBrowserModel::Dispose()
{
  wxLogDebug(_T("Disposing of ObjectBrowser- sending disconnection request to all server databases"));
  std::vector<DatabaseConnection*> disconnecting;
  for (std::list<ServerModel>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    (*iter).BeginDisconnectAll(disconnecting);
  }
  wxLogDebug(_T("Disposing of ObjectBrowser- waiting for %lu database connections to terminate"), disconnecting.size());
  for (std::vector<DatabaseConnection*>::const_iterator iter = disconnecting.begin(); iter != disconnecting.end(); iter++) {
    DatabaseConnection *db = *iter;
    wxLogDebug(_T(" Waiting for database connection %s to exit"), db->Identification().c_str());
    db->WaitUntilClosed();
  }
  wxLogDebug(_T("Disposing of ObjectBrowser- disposing of servers"));
  for (std::list<ServerModel>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    (*iter).Dispose();
  }
  wxLogDebug(_T("Disposing of ObjectBrowser- clearing server list"));
  servers.clear();
}

void ObjectBrowserModel::RemoveServer(const wxString& serverId)
{
  for (std::list<ServerModel>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    if ((*iter).Identification() == serverId) {
      (*iter).Dispose(); // still does nasty synchronous disconnect for now
      servers.erase(iter);
      return;
    }
  }
}

DatabaseModel *ObjectBrowserModel::FindDatabase(const ServerConnection &server, const wxString &dbname)
{
  ServerModel *serverModel = FindServer(server);
  if (serverModel == NULL) return NULL;
  return serverModel->FindDatabase(dbname);
}

DatabaseModel *ObjectBrowserModel::FindDatabase(const ObjectModelReference &ref)
{
  ServerModel *serverModel = FindServer(ref);
  if (serverModel == NULL) return NULL;
  return serverModel->FindDatabase(ref);
}

DatabaseConnection* ServerModel::GetDatabaseConnection(const wxString &dbname)
{
  std::map<wxString, DatabaseConnection*>::const_iterator iter = connections.find(dbname);
  if (iter != connections.end()) {
    DatabaseConnection *db = iter->second;
    if (db->IsAcceptingWork()) {
      wxLogDebug(_T("Using existing connection %s"), db->Identification().c_str());
      return db;
    }
    wxLogDebug(_T("Cleaning stale connection %s"), db->Identification().c_str());
    db->Dispose();
    delete db;
    connections.erase(dbname);
  }
  DatabaseConnection *db = new DatabaseConnection(conninfo, dbname);
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

void ServerModel::BeginDisconnectAll(std::vector<DatabaseConnection*> &disconnecting)
{
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

void ServerModel::Dispose()
{
  wxLogDebug(_T("Disposing of server %s"), Identification().c_str());
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

DatabaseModel *ServerModel::FindDatabase(const wxString &dbname)
{
  for (std::vector<DatabaseModel>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
    if ((*iter).name == dbname)
      return &(*iter);
  }
  return NULL;
}

DatabaseModel *ServerModel::FindDatabaseByOid(Oid oid)
{
  for (std::vector<DatabaseModel>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
    if ((*iter).oid == oid)
      return &(*iter);
  }
  return NULL;
}

ObjectModel *ServerModel::FindObject(const ObjectModelReference& ref)
{
  DatabaseModel *database = FindDatabaseByOid(ref.GetDatabase());
  if (database == NULL) return NULL;
  if (ref.GetObjectClass() == ObjectModelReference::PG_DATABASE)
    return database;
  else
    return database->FindObject(ref);
}

void ServerModel::UpdateServerParameters(const wxString& serverVersionString_, int serverVersion_, SSL *ssl)
{
  serverVersionString = serverVersionString_;
  serverVersion = serverVersion_;
  if (ssl != NULL) {
    sslCipher = wxString(SSL_get_cipher(ssl), wxConvUTF8);
  }
}

void ServerModel::UpdateDatabases(const std::vector<DatabaseModel>& incoming)
{
  databases.clear();
  databases.reserve(incoming.size());
  for (std::vector<DatabaseModel>::const_iterator iter = incoming.begin(); iter != incoming.end(); iter++) {
    databases.push_back(*iter);
    databases.back().server = this;
  }
}

void ServerModel::UpdateDatabase(const DatabaseModel& incoming)
{
  for (std::vector<DatabaseModel>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
    if ((*iter).oid == incoming.oid) {
      wxString dbname = (*iter).name;
      *iter = incoming;
      (*iter).server = this;
      (*iter).name = dbname;
      (*iter).loaded = true;
      return;
    }
  }
  wxASSERT(false);
}

void ServerModel::UpdateRoles(const std::vector<RoleModel>& incoming)
{
  roles.clear();
  roles.reserve(incoming.size());
  for (std::vector<RoleModel>::const_iterator iter = incoming.begin(); iter != incoming.end(); iter++) {
    roles.push_back(*iter);
  }
}

void ServerModel::UpdateTablespaces(const std::vector<TablespaceModel>& incoming)
{
  tablespaces.clear();
  tablespaces.reserve(incoming.size());
  for (std::vector<TablespaceModel>::const_iterator iter = incoming.begin(); iter != incoming.end(); iter++) {
    tablespaces.push_back(*iter);
  }
}

template <typename T>
ObjectModel *SearchContents(std::vector<T>& container, Oid key)
{
  for (typename std::vector<T>::iterator iter = container.begin(); iter != container.end(); iter++) {
    if ((*iter).oid == key)
      return &(*iter);
  }
  return NULL;
}

ObjectModel *DatabaseModel::FindObject(const ObjectModelReference& ref)
{
  switch (ref.GetObjectClass()) {
  case ObjectModelReference::PG_CLASS:
    return SearchContents(relations, ref.GetOid());
  case ObjectModelReference::PG_PROC:
    return SearchContents(functions, ref.GetOid());
  case ObjectModelReference::PG_TS_CONFIG:
    return SearchContents(textSearchConfigurations, ref.GetOid());
  case ObjectModelReference::PG_TS_DICT:
    return SearchContents(textSearchDictionaries, ref.GetOid());
  case ObjectModelReference::PG_TS_PARSER:
    return SearchContents(textSearchParsers, ref.GetOid());
  case ObjectModelReference::PG_TS_TEMPLATE:
    return SearchContents(textSearchTemplates, ref.GetOid());
  default:
    return NULL;
  }
}

DatabaseModel::Divisions DatabaseModel::DivideSchemaMembers() const
{
  std::vector<const SchemaMemberModel*> members;
  for (std::vector<RelationModel>::const_iterator iter = relations.begin(); iter != relations.end(); iter++) {
    members.push_back(&(*iter));
  }
  for (std::vector<FunctionModel>::const_iterator iter = functions.begin(); iter != functions.end(); iter++) {
    members.push_back(&(*iter));
  }
  for (std::vector<TextSearchDictionaryModel>::const_iterator iter = textSearchDictionaries.begin(); iter != textSearchDictionaries.end(); iter++) {
    members.push_back(&(*iter));
  }
  for (std::vector<TextSearchParserModel>::const_iterator iter = textSearchParsers.begin(); iter != textSearchParsers.end(); iter++) {
    members.push_back(&(*iter));
  }
  for (std::vector<TextSearchTemplateModel>::const_iterator iter = textSearchTemplates.begin(); iter != textSearchTemplates.end(); iter++) {
    members.push_back(&(*iter));
  }
  for (std::vector<TextSearchConfigurationModel>::const_iterator iter = textSearchConfigurations.begin(); iter != textSearchConfigurations.end(); iter++) {
    members.push_back(&(*iter));
  }

  sort(members.begin(), members.end(), SchemaMemberModel::CollateByQualifiedName);

  Divisions result;

  for (std::vector<const SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
    const SchemaMemberModel *member = *iter;
    if (!member->extension.IsEmpty())
      result.extensionDivisions[member->extension].push_back(member);
    else if (!member->IsUser())
      result.systemDivision.push_back(member);
    else
      result.userDivision.push_back(member);
  }

  return result;
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
