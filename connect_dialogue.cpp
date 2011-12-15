#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/xrc/xmlres.h"
#include "connect_dialogue.h"
#include "server_connection.h"

BEGIN_EVENT_TABLE(ConnectDialogue, wxDialog)
  EVT_BUTTON(wxID_OK, ConnectDialogue::OnConnect)
  EVT_BUTTON(wxID_CANCEL, ConnectDialogue::OnCancel)
END_EVENT_TABLE()

void ConnectDialogue::OnConnect(wxCommandEvent &event) {
  ServerConnection *conn = new ServerConnection();

  wxString username = usernameInput->GetValue();
  wxString password = passwordInput->GetValue();
  wxString hostname = hostnameInput->GetValue();
  if (!username.IsEmpty()) {
    conn->username = strdup(username.utf8_str());
  }
  if (!password.IsEmpty()) {
    conn->password = strdup(password.utf8_str());
  }
  if (!hostname.IsEmpty()) {
    conn->hostname = strdup(hostname.utf8_str());
  }

  objectBrowser->AddServerConnection(conn);
  Destroy();
}

void ConnectDialogue::OnCancel(wxCommandEvent &event) {
  Destroy();
}
