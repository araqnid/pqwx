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

const int EVENT_CONNECTION_FINISHED = 10000;

BEGIN_EVENT_TABLE(ConnectDialogue, wxDialog)
  EVT_BUTTON(wxID_OK, ConnectDialogue::OnConnect)
  EVT_BUTTON(wxID_CANCEL, ConnectDialogue::OnCancel)
  EVT_COMMAND(EVENT_CONNECTION_FINISHED, wxEVT_COMMAND_TEXT_UPDATED, ConnectDialogue::OnConnectionFinished)
  EVT_COMBOBOX(XRCID("hostname_value"), ConnectDialogue::OnRecentServerChosen)
END_EVENT_TABLE()

class ConnectionWork : public ConnectionCallback {
public:
  ConnectionWork(ConnectDialogue *owner, const ServerConnection &server, DatabaseConnection *db) : owner(owner), server(server), db(db) { }
  void OnConnection(bool usedPassword_) {
    state = CONNECTED;
    usedPassword = usedPassword_;
    notifyFinished();
  }
  void OnConnectionFailed(const wxString &errorMessage_) {
    state = FAILED;
    errorMessage = errorMessage_;
    notifyFinished();
  }
  void OnConnectionNeedsPassword() {
    state = NEEDS_PASSWORD;
    notifyFinished();
  }
  enum { CONNECTED, FAILED, NEEDS_PASSWORD } state;
  wxString errorMessage;
private:
  void notifyFinished() {
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_CONNECTION_FINISHED);
    event.SetClientData(this);
    owner->AddPendingEvent(event);
  }
  ConnectDialogue *owner;
  ServerConnection server;
  DatabaseConnection *db;
  bool usedPassword;
  friend class ConnectDialogue;
};

#ifdef USE_DEBIAN_PGCLUSTER

#define PGCLUSTER_BINDIR "/usr/lib/postgresql"
#define PGCLUSTER_CONFDIR "/etc/postgresql"

#include "wx/regex.h"
#include <fstream>
#include <unistd.h>
#include <dirent.h>
#include "pg_config_manual.h"

static wxRegEx clusterPattern(_T("([0-9]+\\.[0-9]+)/(.+)"));
static wxRegEx remoteClusterPattern(_T("([^:]+):([0-9]*)"));
static wxRegEx settingPattern(_T("^([a-z_]+) *= *(.+)"));
static wxRegEx versionPattern(_T("[0-9]+\\.[0-9]+"));

static wxString ReadConfigValue(const wxString &filename, const wxString &keyword) {
  std::ifstream input(filename.fn_str());
  if (!input.is_open()) {
    return wxEmptyString;
  }

  std::string line;
  while (input.good()) {
    getline(input, line);
    wxString recoded(line.c_str(), wxConvUTF8);
    if (settingPattern.Matches(recoded) && settingPattern.GetMatch(recoded, 1) == keyword) {
      wxString value = settingPattern.GetMatch(recoded, 2);

      int comment = value.Find(_T('#'));
      if (comment != wxNOT_FOUND) value = value.Left(comment);

      value = value.Trim();
      if (value[0] == _T('\'') && value[value.length() - 1] == _T('\'')) {
	wxString unquoted;
	for (size_t pos = 1; pos < (value.length() - 1); pos++) {
	  if (value[pos] == _T('\'') && value[pos+1] == _T('\'')) {
	    unquoted << _T('\'');
	    pos++;
	  }
	  else {
	    unquoted << value[pos];
	  }
	}
	return unquoted;
      }
      else
	return value;
    }
  }

  return wxEmptyString;
}

static wxString ParseCluster(const wxString &server) {
  if (!clusterPattern.Matches(server))
    return server;
  wxString clusterName = clusterPattern.GetMatch(server, 2);

  if (remoteClusterPattern.Matches(clusterName)) {
    wxLogDebug(_T("%s looks like a remote cluster"), server.c_str());
    if (remoteClusterPattern.GetMatch(clusterName, 2).IsEmpty())
      return remoteClusterPattern.GetMatch(clusterName, 1);
    else
      return clusterName;
  }

  wxString localConfigFile = _T(PGCLUSTER_CONFDIR "/") + clusterPattern.GetMatch(server, 1) + _T("/") + clusterName + _T("/postgresql.conf");
  wxString portString = ReadConfigValue(localConfigFile, _T("port"));

  if (portString.IsEmpty()) {
    wxLogError(_("Failed to find a 'port' setting in %s"), localConfigFile.c_str());
    return wxEmptyString;
  }

  long port;
  if (!portString.ToLong(&port)) {
    wxLogError(_("Incomprehensible 'port' setting in %s: \"%s\""), localConfigFile.c_str(), portString.c_str());
    return wxEmptyString;
  }

  wxString unixSocketDirectory = ReadConfigValue(localConfigFile, _T("unix_socket_directory"));

  wxString localServer;
  if (!unixSocketDirectory.IsEmpty() && unixSocketDirectory != _T(DEFAULT_PGSOCKET_DIR)) {
    localServer << unixSocketDirectory;
  }
  localServer << _T(':');
  if (port != DEF_PGPORT)
    localServer << port;

  return localServer;
}

struct PgCluster {
  wxString version;
  wxString name;
  int port;
};

static std::vector<wxString> ClusterVersions() {
  std::vector<wxString> versions;
  wxLogDebug(_T("Scanning " PGCLUSTER_BINDIR " for versions"));
  DIR *dir = opendir(PGCLUSTER_BINDIR);
  if (dir != NULL) {
    size_t len = offsetof(struct dirent, d_name) + pathconf(PGCLUSTER_BINDIR, _PC_NAME_MAX) + 1;
    struct dirent *entryData = (struct dirent*) malloc(len);
    struct dirent *result;
    do {
      if (readdir_r(dir, entryData, &result) != 0) {
	wxLogSysError(_T("Unable to read " PGCLUSTER_BINDIR " directory"));
	break;
      }
      if (result == NULL)
	break;
      wxString filename(entryData->d_name, wxConvUTF8);
      if (versionPattern.Matches(filename)) {
	wxLogDebug(_T(" Found '%s'"), filename.c_str());
	versions.push_back(filename);
      }
      else {
	wxLogDebug(_T(" Ignored '%s'"), filename.c_str());
      }
    } while (1);
    closedir(dir);
    free(entryData);
  }
#ifdef __WXDEBUG__
  else {
    wxLogSysError(_T("Unable to read " PGCLUSTER_BINDIR " directory"));
  }
#endif
  return versions;
}

static std::vector<PgCluster> VersionClusters(const wxString &version) {
  std::vector<PgCluster> clusters;
  wxString versionDir = _T(PGCLUSTER_CONFDIR "/") + version + _T("/");
  DIR *dir = opendir(versionDir.fn_str());
  if (dir != NULL) {
    size_t len = offsetof(struct dirent, d_name) + pathconf(PGCLUSTER_BINDIR, _PC_NAME_MAX) + 1;
    struct dirent *entryData = (struct dirent*) malloc(len);
    struct dirent *result;
    do {
      if (readdir_r(dir, entryData, &result) != 0) {
	wxLogSysError(_T("Unable to read '%s' directory"), versionDir.c_str());
	break;
      }
      if (result == NULL)
	break;
      wxString filename(entryData->d_name, wxConvUTF8);
      wxString configFile = versionDir + filename + _T("/postgresql.conf");
      struct stat statbuf;
      if (stat(configFile.fn_str(), &statbuf) == 0) {
	PgCluster cluster;
	cluster.version = version;
	cluster.name = filename;
	wxString portString = ReadConfigValue(configFile, _T("port"));
	long port;
	if (!portString.ToLong(&port)) {
	  wxLogError(_("Incomprehensible 'port' setting in %s: \"%s\""), configFile.c_str(), portString.c_str());
	  continue;
	}
	cluster.port = (int) port;
	clusters.push_back(cluster);
      }
    } while (1);
    closedir(dir);
    free(entryData);
  }
#ifdef __WXDEBUG__
  else {
    wxLogSysError(_T("Unable to read '%s' directory"), versionDir.c_str());
  }
#endif
  return clusters;
}

static std::vector<PgCluster> LocalClusters() {
  std::vector<PgCluster> clusters;
  std::vector<wxString> versions = ClusterVersions();
  for (std::vector<wxString>::const_iterator iter = versions.begin(); iter != versions.end(); iter++) {
    wxLogDebug(_T("Finding clusters for version %s"), (*iter).c_str());
    std::vector<PgCluster> versionClusters = VersionClusters(*iter);
    for (std::vector<PgCluster>::const_iterator clusterIter = versionClusters.begin(); clusterIter != versionClusters.end(); clusterIter++) {
      wxLogDebug(_T(" Found %s on port %d"), (*clusterIter).version.c_str(), (*clusterIter).port);
      clusters.push_back(*clusterIter);
    }
  }
  return clusters;
}

static wxString DefaultCluster(const std::vector<PgCluster> &clusters) {
  // TODO check $HOME/.postgresqlrc
  // TODO check /etc/postgresql-common/user_clusters

  for (std::vector<PgCluster>::const_iterator iter = clusters.begin(); iter != clusters.end(); iter++) {
    if ((*iter).port == DEF_PGPORT) {
      wxLogDebug(_T("Using cluster %s/%s on default port"), (*iter).version.c_str(), (*iter).name.c_str());
      return (*iter).version + _T("/") + (*iter).name;
    }
  }

  if (!clusters.empty()) {
    wxLogDebug(_T("Using first cluster, %s/%s"), clusters[0].version.c_str(), clusters[0].name.c_str());
    return clusters[0].version + _T("/") + clusters[0].name;
  }

  return wxEmptyString;
}

#endif

void ConnectDialogue::StartConnection() {
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
    wxString resolvedHostname = ParseCluster(hostname);
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

  DatabaseConnection *db = new DatabaseConnection(server, server.globalDbName);
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
    delete connection;
    connection = NULL;
  }
  callback->Cancelled();
  delete callback;
  Destroy();
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

void ConnectDialogue::DoInitialConnection(const ServerConnection &conninfo)
{
  hostnameInput->SetValue(conninfo.identifiedAs);
  usernameInput->SetValue(conninfo.username);
  passwordInput->SetValue(conninfo.password);
  StartConnection();
}

void ConnectDialogue::OnConnectionFinished(wxCommandEvent &event) {
  ConnectionWork *work = static_cast<ConnectionWork*>(event.GetClientData());
  UnmarkBusy();
  if (cancelling)
    return;

  if (work->state == ConnectionWork::CONNECTED) {
    work->server.passwordNeededToConnect = work->usedPassword;
    if (!passwordInput->GetValue().empty()) {
      if (!work->usedPassword)
	wxMessageBox(_("You supplied a password to connect to the server, but the connection was successfully made to the server without using it."));
    }
    SaveRecentServers();
    ServerConnection &server = work->server;
    if (!savePasswordInput->IsChecked()) {
      server.password = wxEmptyString;
    }
    callback->Connected(server, work->db);
    delete callback;
    Destroy();
  }
  else if (work->state == ConnectionWork::NEEDS_PASSWORD) {
    wxLogError(_("You must enter a password to connect to this server."), work->errorMessage.c_str());
  }
  else if (work->state == ConnectionWork::FAILED) {
    wxLogError(_("Connection to server failed.\n\n%s"), work->errorMessage.c_str());
  }

  delete work;
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

  for (std::list<RecentServerParameters>::iterator iter = recentServerList.begin(); iter != recentServerList.end(); iter++) {
    hostnameInput->Append(iter->server);
  }

#ifdef USE_DEBIAN_PGCLUSTER
  std::vector<PgCluster> localClusters = LocalClusters();
  for (std::vector<PgCluster>::const_iterator iter = localClusters.begin(); iter != localClusters.end(); iter++) {
    wxString clusterName = (*iter).version + _T("/") + (*iter).name;
    if (hostnameInput->FindString(clusterName) == wxNOT_FOUND) {
      hostnameInput->Append(clusterName);
    }
  }

  wxString defaultCluster = DefaultCluster(localClusters);
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
