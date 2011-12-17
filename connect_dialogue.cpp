#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/xrc/xmlres.h"
#include "pqwx.h"
#include "connect_dialogue.h"
#include "server_connection.h"

const int EVENT_CONNECTION_FINISHED = 10000;

BEGIN_EVENT_TABLE(ConnectDialogue, wxDialog)
  EVT_BUTTON(wxID_OK, ConnectDialogue::OnConnect)
  EVT_BUTTON(wxID_CANCEL, ConnectDialogue::OnCancel)
  EVT_COMMAND(EVENT_CONNECTION_FINISHED, wxEVT_COMMAND_TEXT_UPDATED, ConnectDialogue::OnConnectionFinished)
END_EVENT_TABLE()

class ConnectionWork : public ConnectionCallback {
public:
  ConnectionWork(ConnectDialogue *owner, ServerConnection *server, DatabaseConnection *db) : owner(owner), server(server), db(db) { }
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
  ServerConnection *server;
  DatabaseConnection *db;
  bool usedPassword;
  friend class ConnectDialogue;
};

void ConnectDialogue::StartConnection() {
  ServerConnection *server = new ServerConnection();
  wxString username = usernameInput->GetValue();
  wxString password = passwordInput->GetValue();
  wxString hostname = hostnameInput->GetValue();
  if (!username.IsEmpty()) {
    server->username = username;
  }
  if (!password.IsEmpty()) {
    server->password = password;
  }
  if (!hostname.IsEmpty()) {
    server->SetServerName(hostname);
  }

  wxASSERT(connection == NULL);

  DatabaseConnection *db = new DatabaseConnection(server, server->globalDbName);
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
  Destroy();
}

void ConnectDialogue::DoInitialConnection(const wxString& server, const wxString& user, const wxString& password) {
  hostnameInput->SetValue(server);
  usernameInput->SetValue(user);
  passwordInput->SetValue(password);
  StartConnection();
}

void ConnectDialogue::OnConnectionFinished(wxCommandEvent &event) {
  ConnectionWork *work = static_cast<ConnectionWork*>(event.GetClientData());
  UnmarkBusy();
  if (cancelling)
    return;

  if (work->state == ConnectionWork::CONNECTED) {
    objectBrowser->AddServerConnection(work->server, work->db);
    if (!passwordInput->GetValue().empty()) {
      if (work->usedPassword) {
	if (savePasswordInput->GetValue())
	  SaveServerPassword(work->server);
      }
      else
	wxMessageBox(_("You supplied a password to connect to the server, but the connection was successfully made to the server without using it."));
    }
    SaveRecentServer();
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

void ConnectDialogue::SaveServerPassword(ServerConnection *conn) {
  if (conn->password.IsEmpty())
    return;
  wxLogDebug(_T("Save password for hostname=%s port=%d user=%s..."), conn->hostname.c_str(), conn->port, conn->username.c_str());
}

void ConnectDialogue::SaveRecentServer() {
  wxString server = hostnameInput->GetValue();
  wxString username = usernameInput->GetValue();

  // ":" is interpreted the same way as an empty string
  if (server == _T(":"))
    server = wxEmptyString;

  list<wxString> recentServerList = PQWXApp::LoadConfigList(_T("/ConnectDialogue/RecentServers"));

  recentServerList.remove(server);

  recentServerList.push_front(server);

  if (recentServerList.size() > maxRecentServers) {
    recentServerList.resize(maxRecentServers);
  }

  PQWXApp::SaveConfigList(_T("/ConnectDialogue/RecentServers"), recentServerList);
}

void ConnectDialogue::LoadRecentServers() {
  list<wxString> recentServerList = PQWXApp::LoadConfigList(_T("/ConnectDialogue/RecentServers"));
  if (recentServerList.empty()) return;

  for (list<wxString>::iterator iter = recentServerList.begin(); iter != recentServerList.end(); iter++) {
    hostnameInput->Append(*iter);
  }

  hostnameInput->SetValue(recentServerList.front());
}