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

extern void InitXmlResource(void);
extern void InitObjectFinderResources(void);

IMPLEMENT_APP(PQWXApp)

bool PQWXApp::OnInit()
{
  if (!wxApp::OnInit())
    return false;

  InitXmlResource();
  wxXmlResource::Get()->InitAllHandlers();

  InitObjectFinderResources();
  wxImage::AddHandler(new wxPNGHandler());

  PqwxFrame *frame = new PqwxFrame(_T("PQWX"));
  frame->Show(true);

  ConnectDialogue *connect = new ConnectDialogue(NULL, new PqwxFrame::AddConnectionToObjectBrowser(frame));
  connect->Show();
  if (haveInitial) {
    ServerConnection server;
    server.identifiedAs = initialServer;
    server.username = initialUser;
    server.password = initialPassword;
    connect->DoInitialConnection(server);
  }

  return true;
}

void PQWXApp::OnInitCmdLine(wxCmdLineParser &parser) {
  parser.AddOption(_T("S"), _T("server"), _("server host (or host:port)"));
  parser.AddOption(_T("U"), _T("user"), _("login username"));
  parser.AddOption(_T("P"), _T("password"), _("login password"));
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

  return true;
}
