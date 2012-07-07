#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <list>
#include "wx/xrc/xmlres.h"
#include "wx/config.h"
#include "pqwx.h"
#include "connect_dialogue.h"
#include "server_connection.h"

BEGIN_EVENT_TABLE(ConnectDialogue, wxDialog)
  EVT_BUTTON(wxID_OK, ConnectDialogue::OnConnect)
  EVT_BUTTON(wxID_CANCEL, ConnectDialogue::OnCancel)
  PQWX_CONNECTION_ATTEMPT_COMPLETED(wxID_ANY, ConnectDialogue::OnConnectionFinished)
  EVT_COMBOBOX(XRCID("hostname_value"), ConnectDialogue::OnRecentServerChosen)
END_EVENT_TABLE()

DEFINE_LOCAL_EVENT_TYPE(PQWX_ConnectionAttemptCompleted)

void ConnectDialogue::StartConnection(const wxString &dbname) {
  ServerConnection server;
  wxString username = usernameInput->GetValue();
  wxString password = passwordInput->GetValue();
  wxString hostname = hostnameInput->GetValue();

  if (!username.IsEmpty()) {
    server.username = username;
  }
  if (!password.IsEmpty()) {
    server.password = password;
  }
  if (!hostname.IsEmpty()) {
#ifdef USE_DEBIAN_PGCLUSTER
    wxString resolvedHostname = clusters.ParseCluster(hostname);
    if (resolvedHostname.IsEmpty())
      return;
    if (resolvedHostname == hostname)
      server.SetServerName(hostname);
    else {
      server.SetServerName(resolvedHostname, hostname);
    }
#else
    server.SetServerName(hostname);
#endif
  }
  else {
    server.SetServerName(wxEmptyString);
  }

  wxASSERT(connection == NULL);

  DatabaseConnection *db = new DatabaseConnection(server, dbname);
  connection = new ConnectionWork(this, server, db);
  db->Connect(connection);
  MarkBusy();
}

void ConnectDialogue::OnCancel(wxCommandEvent &event) {
  cancelButton->Disable();
  cancelling = true;
  if (connection != NULL) {
    // TODO this merely waits (synchronously!) for the connection attempt to end, instead of actively cancelling it
    connection->db->CloseSync();
    // the callback will then exit
    return;
  }
  if (callback) {
    callback->Cancelled();
    delete callback;
    Destroy();
  }
  else {
    EndModal(wxID_CANCEL);
  }
}

void ConnectDialogue::Suggest(const ServerConnection &conninfo)
{
  if (!conninfo.identifiedAs.empty())
    hostnameInput->SetValue(conninfo.identifiedAs);
  else if (conninfo.port > 0)
    hostnameInput->SetValue(wxString::Format(_T("%s:%d"), conninfo.hostname.c_str(), conninfo.port));
  else
    hostnameInput->SetValue(conninfo.hostname);
  usernameInput->SetValue(conninfo.username);
  passwordInput->SetValue(conninfo.password);
}

void ConnectDialogue::DoInitialConnection(const ServerConnection &conninfo, const wxString &dbname)
{
  Suggest(conninfo);
  StartConnection(dbname);
}

void ConnectDialogue::OnConnectionFinished(wxCommandEvent &event) {
  wxASSERT(connection != NULL);

  UnmarkBusy();
  if (cancelling) {
    if (connection->db)
      delete connection->db;
    delete connection;
    if (callback) {
      callback->Cancelled();
      delete callback;
      Destroy();
    }
    else {
      EndModal(wxID_CANCEL);
    }
    return;
  }

  if (connection->state == ConnectionWork::CONNECTED) {
    connection->server.passwordNeededToConnect = connection->usedPassword;
    if (!passwordInput->GetValue().empty()) {
      if (!connection->usedPassword)
        wxMessageBox(_("You supplied a password to connect to the server, but the connection was successfully made to the server without using it."));
    }
    SaveRecentServers();
    server = connection->server;
    db = connection->db;
    if (!savePasswordInput->IsChecked()) {
      server.password = wxEmptyString;
    }
    if (callback) {
      callback->Connected(server, connection->db);
      delete callback;
      Destroy();
    }
    else {
      EndModal(wxID_OK);
    }
  }
  else if (connection->state == ConnectionWork::NEEDS_PASSWORD) {
    wxLogError(_("You must enter a password to connect to this server."), connection->error.GetPrimary().c_str());
    connection->db->Dispose();
    delete connection->db;
  }
  else if (connection->state == ConnectionWork::FAILED) {
    wxLogError(_("Connection to server failed.\n\n%s"), connection->error.GetPrimary().c_str());
    connection->db->Dispose();
    delete connection->db;
  }

  delete connection;
  connection = NULL;
}

void ConnectDialogue::MarkBusy() {
  usernameInput->Disable();
  passwordInput->Disable();
  hostnameInput->Disable();
  savePasswordInput->Disable();
  okButton->Disable();
  wxBeginBusyCursor();
}

void ConnectDialogue::UnmarkBusy() {
  usernameInput->Enable();
  passwordInput->Enable();
  hostnameInput->Enable();
  savePasswordInput->Enable();
  okButton->Enable();
  wxEndBusyCursor();
}

void ConnectDialogue::SaveRecentServers() {
  RecentServerParameters server;
  server.server = hostnameInput->GetValue();
  server.username = usernameInput->GetValue();
  if (savePasswordInput->IsChecked()) {
    server.password = passwordInput->GetValue();
  }

  // ":" is interpreted the same way as an empty string
  if (server.server == _T(":"))
    server.server = wxEmptyString;

  ReadRecentServers(); // in case something external changed it?

  recentServerList.remove(server);

  recentServerList.push_front(server);

  if (recentServerList.size() > maxRecentServers) {
    recentServerList.resize(maxRecentServers);
  }

  WriteRecentServers();
}

void ConnectDialogue::LoadRecentServers() {
  ReadRecentServers();

  // setup server list

  for (std::list<RecentServerParameters>::iterator iter = recentServerList.begin(); iter != recentServerList.end(); iter++) {
    hostnameInput->Append(iter->server);
  }

#ifdef USE_DEBIAN_PGCLUSTER
  const std::vector<PgCluster>& localClusters = clusters.LocalClusters();
  for (std::vector<PgCluster>::const_iterator iter = localClusters.begin(); iter != localClusters.end(); iter++) {
    wxString clusterName = (*iter).version + _T("/") + (*iter).name;
    if (hostnameInput->FindString(clusterName) == wxNOT_FOUND) {
      hostnameInput->Append(clusterName);
    }
  }
#endif

  std::list<NearbyServersRegistry::ServerInfo> nearbyServers = ::wxGetApp().GetNearbyServersRegistry().GetDiscoveredServers();
  for (std::list<NearbyServersRegistry::ServerInfo>::iterator iter = nearbyServers.begin(); iter != nearbyServers.end(); iter++) {
    wxString serverName = (*iter).name;
    wxLogDebug(_T("Nearby server: %s"), serverName.c_str());
    if (hostnameInput->FindString(serverName) == wxNOT_FOUND) {
      hostnameInput->Append(serverName);
    }
  }

  // pick default server

#ifdef USE_DEBIAN_PGCLUSTER
  wxString defaultCluster = clusters.DefaultCluster();

  if (!defaultCluster.IsEmpty()) {
    // is the default cluster in the recent server list? if so, pick up its additional parameters
    std::list<RecentServerParameters>::const_iterator defaultAsRecentServer = std::find(recentServerList.begin(), recentServerList.end(), defaultCluster);
    if (defaultAsRecentServer != recentServerList.end()) {
      wxLogDebug(_T("Taking default cluster parameters from recent server list"));
      LoadRecentServer(*defaultAsRecentServer);
    }
    else {
      wxLogDebug(_T("Using default cluster with default parameters"));
      hostnameInput->SetValue(defaultCluster);
      usernameInput->SetValue(wxEmptyString);
      passwordInput->SetValue(wxEmptyString);
      savePasswordInput->SetValue(false);
    }
    return;
  }
#endif

  if (!recentServerList.empty())
    LoadRecentServer(recentServerList.front());
}

void ConnectDialogue::LoadRecentServer(const RecentServerParameters &server) {
  hostnameInput->SetValue(server.server);
  usernameInput->SetValue(server.username);
  passwordInput->SetValue(server.password);
  savePasswordInput->SetValue(!server.password.IsEmpty());
}

void ConnectDialogue::ReadRecentServers() {
  wxConfigBase *cfg = wxConfig::Get();
  wxString oldPath = cfg->GetPath();
  cfg->SetPath(recentServersConfigPath);

  recentServerList.clear();

  int pos = 0;
  do {
    wxString key = wxString::Format(_T("%d"), pos++);
    RecentServerParameters server;
    if (!cfg->Read(key + _T("/Server"), &server.server))
      break;
    cfg->Read(key + _T("/Username"), &server.username);
    cfg->Read(key + _T("/Password"), &server.password);
    recentServerList.push_back(server);
  } while (1);

  cfg->SetPath(oldPath);
}

void ConnectDialogue::WriteRecentServers() {
  wxConfigBase *cfg = wxConfig::Get();
  wxString oldPath = cfg->GetPath();
  cfg->SetPath(recentServersConfigPath);

  int pos = 0;
  for (std::list<RecentServerParameters>::const_iterator iter = recentServerList.begin(); iter != recentServerList.end(); iter++, pos++) {
    wxString key = wxString::Format(_T("%d"), pos);
    cfg->Write(key + _T("/Server"), iter->server);
    if (!iter->username.IsEmpty())
      cfg->Write(key + _T("/Username"), iter->username);
    else
      cfg->DeleteEntry(key + _T("/Username"));
    if (!iter->password.IsEmpty())
      cfg->Write(key + _T("/Password"), iter->password);
    else
      cfg->DeleteEntry(key + _T("/Password"));
  }

  do {
    wxString key = wxString::Format(_T("%d"), pos++);
    if (!cfg->Exists(key))
        break;
    wxLogDebug(_T("Pruning configuration group \"%s\""), key.c_str());
    if (!cfg->DeleteGroup(key))
      break;
  } while (1);

  cfg->SetPath(oldPath);
}

void ConnectDialogue::OnRecentServerChosen(wxCommandEvent& event) {
  wxString newServer = hostnameInput->GetValue();
  for (std::list<RecentServerParameters>::iterator iter = recentServerList.begin(); iter != recentServerList.end(); iter++) {
    if (iter->server == newServer) {
      LoadRecentServer(*iter);
      return;
    }
  }

#ifdef USE_DEBIAN_PGCLUSTER
  // cluster hint
  hostnameInput->SetValue(newServer);
  usernameInput->SetValue(wxEmptyString);
  passwordInput->SetValue(wxEmptyString);
  savePasswordInput->SetValue(false);
#else
  wxASSERT(false);
#endif
}
// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
