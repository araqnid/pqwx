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

ServerModel *ObjectBrowserModel::FindServerById(const wxString &serverId) const
{
  for (std::list<ServerModel*>::const_iterator iter = servers.begin(); iter != servers.end(); iter++) {
    ServerModel *serverModel = *iter;
    if (serverModel->Identification() == serverId) {
      return serverModel;
    }
  }

  return NULL;
}

ServerModel* ObjectBrowserModel::AddServerConnection(const ServerConnection &server, DatabaseConnection *db)
{
  ServerModel *serverModel;
  if (db) {
    serverModel = new ServerModel(server, db);
    if (db->IsConnected()) {
      SetupDatabaseConnection(db);
    }
  }
  else {
    serverModel = new ServerModel(server);
  }

  servers.push_back(serverModel);

  return serverModel;
}

void ObjectBrowserModel::SetupDatabaseConnection(DatabaseConnection *db)
{
  db->AddWork(new ObjectBrowserDatabaseWork(wxTheApp, new SetupDatabaseConnectionWork()));
}

void ObjectBrowserModel::Dispose()
{
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

void ObjectBrowserModel::RemoveServer(ServerModel *server)
{
  servers.remove(server);
  server->Dispose(); // still does nasty synchronous disconnect for now
}

DatabaseModel *ObjectBrowserModel::FindDatabase(const ServerConnection &server, const wxString &dbname) const
{
  ServerModel *serverModel = FindServer(server);
  if (serverModel == NULL) return NULL;
  return serverModel->FindDatabase(dbname);
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

inline DatabaseModel *ServerModel::FindDatabase(const wxString &dbname) const
{
  for (std::vector<DatabaseModel*>::const_iterator iter = databases.begin(); iter != databases.end(); iter++) {
    if ((*iter)->name == dbname)
      return *iter;
  }
  return NULL;
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
