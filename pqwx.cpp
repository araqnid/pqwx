#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/cmdline.h"
#include "wx/xrc/xmlres.h"
#include "pqwx.h"
#include "pqwx_frame.h"
#include "connect_dialogue.h"
#include "object_finder.h"
#include "static_resources.h"
#include "database_notification_monitor.h"

extern void InitXmlResource(void);
extern void InitStaticResources(void);

#ifdef PQWX_NOTIFICATION_MONITOR
DatabaseNotificationMonitor* PQWXApp::monitor;

DatabaseNotificationMonitor* PQWXApp::GetNotificationMonitor()
{
  return PQWXApp::monitor;
}
#endif

IMPLEMENT_APP(PQWXApp)

bool PQWXApp::OnInit()
{
  if (!wxApp::OnInit())
    return false;

  InitXmlResource();
  wxXmlResource::Get()->InitAllHandlers();

  StaticResources::Init();

#ifdef PQWX_NOTIFICATION_MONITOR
  monitor = new DatabaseNotificationMonitor();
#endif

#ifdef USE_DEBIAN_PGCLUSTER
  toolsRegistry.SetUseSystemPath(false);
  toolsRegistry.AddSuggestion(_T("/usr/lib/postgresql"));
#endif

  toolsRegistry.BeginFindInstallations();

  PqwxFrame *frame = new PqwxFrame(_T("PQWX"));
  frame->Show(true);

  ConnectDialogue connect(frame);
  if (haveInitial) {
    ServerConnection server;
    server.identifiedAs = initialServer;
    server.username = initialUser;
    server.password = initialPassword;
    connect.DoInitialConnection(server);
  }
  if (connect.ShowModal() == wxID_OK) {
    frame->GetObjectBrowser()->AddServerConnection(connect.GetServerParameters(), connect.GetConnection());
    wxString dbname = initialDatabase.empty() ? connect.GetServerParameters().globalDbName : initialDatabase;
    for (std::vector<wxString>::const_iterator iter = initialFiles.begin(); iter != initialFiles.end(); iter++) {
      frame->OpenScript(*iter, connect.GetServerParameters(), dbname);
    }
  }

  return true;
}

int PQWXApp::OnExit()
{
  int rc = wxApp::OnExit();

#ifdef PQWX_NOTIFICATION_MONITOR
  delete monitor;
#endif

  return rc;
}

void PQWXApp::OnInitCmdLine(wxCmdLineParser &parser) {
  parser.AddOption(_T("S"), _T("server"), _("server host (or host:port)"));
  parser.AddOption(_T("U"), _T("user"), _("login username"));
  parser.AddOption(_T("P"), _T("password"), _("login password"));
  parser.AddOption(_T("d"), _T("database"), _("database to connect to"));
  parser.AddParam(_T("file.sql"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL);
  wxApp::OnInitCmdLine(parser);
}

bool PQWXApp::OnCmdLineParsed(wxCmdLineParser &parser) {
  haveInitial = false;

  if (parser.Found(_T("S"), &initialServer)) {
    haveInitial = true;
  }
  if (parser.Found(_T("U"), &initialUser)) {
    haveInitial = true;
  }
  if (parser.Found(_T("P"), &initialPassword)) {
    haveInitial = true;
  }
  
  parser.Found(_T("d"), &initialDatabase);

  int params = parser.GetParamCount();
  for (int i = 0; i < params; i++) {
    initialFiles.push_back(parser.GetParam(i));
  }

  return true;
}
