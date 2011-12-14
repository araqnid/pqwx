#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/cmdline.h"
#include "pqwx_frame.h"
#include "server_connection.h"

class PQWXApp: public wxApp {
public:
  virtual bool OnInit();
  void OnInitCmdLine(wxCmdLineParser &parser);
  bool OnCmdLineParsed(wxCmdLineParser &parser);
private:
  ServerConnection *initialConnection;
};

IMPLEMENT_APP(PQWXApp)

bool PQWXApp::OnInit()
{
  if (!wxApp::OnInit())
    return false;

  PqwxFrame *frame = new PqwxFrame(_T("PQWX"));
  frame->Show(true);

  if (initialConnection)
    frame->objectBrowser->AddServerConnection(initialConnection);

  return true;
}

void PQWXApp::OnInitCmdLine(wxCmdLineParser &parser) {
  parser.AddOption(_T("S"), _T("server"), _("server host (or host:port)"));
  parser.AddOption(_T("U"), _T("user"), _("login username"));
  parser.AddOption(_T("P"), _T("password"), _("login password"));
  wxApp::OnInitCmdLine(parser);
}

bool PQWXApp::OnCmdLineParsed(wxCmdLineParser &parser) {
  wxString server, user, password;
  bool haveInitial = false;

  if (parser.Found(_T("S"), &server)) {
    haveInitial = true;
  }
  if (parser.Found(_T("U"), &user)) {
    haveInitial = true;
  }
  if (parser.Found(_T("P"), &password)) {
    haveInitial = true;
  }

  if (haveInitial) {
    initialConnection = new ServerConnection();
    if (!server.IsEmpty()) {
      int colon = server.Find(_T(':'));
      if (colon == wxNOT_FOUND) {
	initialConnection->hostname = strdup(server.utf8_str());
      }
      else {
	initialConnection->hostname = strdup(server.Mid(0, colon).utf8_str());
	initialConnection->port = atoi(server.Mid(colon+1).utf8_str());
      }
    }
    if (!user.IsEmpty()) {
      initialConnection->username = strdup(user.utf8_str());
    }
    if (!password.IsEmpty()) {
      initialConnection->password = strdup(password.utf8_str());
    }
  }
  else {
    initialConnection = NULL;
  }

  return true;
}
