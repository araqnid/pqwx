#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/toolbar.h"
#include "wx/aboutdlg.h"

#include "pqwx_frame.h"
#include "pqwx_version.h"
#include "object_browser.h"
#include "connect_dialogue.h"

#if !defined(__WXMSW__) && !defined(__WXPM__)
    #include "pqwx-appicon.xpm"
#endif

#include "toolbar-new.xpm"

BEGIN_EVENT_TABLE(PqwxFrame, wxFrame)
  EVT_MENU(Pqwx_Quit, PqwxFrame::OnQuit)
  EVT_MENU(Pqwx_About, PqwxFrame::OnAbout)
  EVT_MENU(Pqwx_ConnectObjectBrowser, PqwxFrame::OnConnectObjectBrowser)
  EVT_MENU(Pqwx_DisconnectObjectBrowser, PqwxFrame::OnDisconnectObjectBrowser)
  EVT_CLOSE(PqwxFrame::OnCloseFrame)
END_EVENT_TABLE()

const int TOOLBAR_MAIN = 500;

PqwxFrame::PqwxFrame(const wxString& title)
  : wxFrame(NULL, wxID_ANY, title)
{
  SetIcon(wxICON(Pqwx_appicon));

  wxMenu *fileMenu = new wxMenu;
  fileMenu->Append(Pqwx_ConnectObjectBrowser, _T("Connect Object &Browser..."), _T("Open the database schema browsing tool"));
  fileMenu->Append(Pqwx_DisconnectObjectBrowser, _T("&Disconnect Object Browser"), _T("Disconnect the schema browsing tool"));
  fileMenu->Append(Pqwx_Quit, _T("E&xit\tCtrl-Q"), _T("Exit PQWX"));

  wxMenu *helpMenu = new wxMenu;
  helpMenu->Append(Pqwx_About, _T("&About...\tF1"), _T("Show information about PQWX"));

  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(fileMenu, _T("&File"));
  menuBar->Append(helpMenu, _T("&Help"));

  SetMenuBar(menuBar);

  CreateStatusBar(2);

  objectBrowser = new ObjectBrowser(this, Pqwx_ObjectBrowser);

  SetPosition(wxPoint(100,100));
  SetSize(wxSize(1000,800));
}

void PqwxFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
  Close(true);
}

void PqwxFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
  wxAboutDialogInfo info;
  info.SetName(_T("PQWX"));
  info.SetVersion(_T(PQWX_VERSION));
  info.SetDescription(_T("PostgreSQL query tool"
#ifdef PQWX_DEBUG
			 _T(" - Debug build")
#endif
			 ));
  info.SetCopyright(_T("(c) 2011 Steve Haslam"));
  wxAboutBox(info);
}

void PqwxFrame::OnConnectObjectBrowser(wxCommandEvent& event)
{
  wxDialog *connect = new ConnectDialogue(NULL, objectBrowser);
  connect->Show();
}

void PqwxFrame::OnDisconnectObjectBrowser(wxCommandEvent& event)
{
}

void PqwxFrame::OnCloseFrame(wxCloseEvent& event) {
  objectBrowser->Dispose();
  Destroy();
}
