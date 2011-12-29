#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/aboutdlg.h"
#include "wx/listbox.h"
#include "wx/notebook.h"

#include "pqwx_frame.h"
#include "pqwx_version.h"
#include "wx_flavour.h"
#include "object_browser.h"
#include "connect_dialogue.h"

#if !defined(__WXMSW__) && !defined(__WXPM__)
    #include "pqwx-appicon.xpm"
#endif

BEGIN_EVENT_TABLE(PqwxFrame, wxFrame)
  EVT_MENU(wxID_EXIT, PqwxFrame::OnQuit)
  EVT_MENU(wxID_ABOUT, PqwxFrame::OnAbout)
  EVT_MENU(XRCID("ConnectObjectBrowser"), PqwxFrame::OnConnectObjectBrowser)
  EVT_MENU(XRCID("DisconnectObjectBrowser"), PqwxFrame::OnDisconnectObjectBrowser)
  EVT_MENU(XRCID("FindObject"), PqwxFrame::OnFindObject)
  EVT_CLOSE(PqwxFrame::OnCloseFrame)
END_EVENT_TABLE()

const int TOOLBAR_MAIN = 500;

PqwxFrame::PqwxFrame(const wxString& title)
  : wxFrame(NULL, wxID_ANY, title)
{
  SetIcon(wxICON(Pqwx_appicon));

  SetMenuBar(wxXmlResource::Get()->LoadMenuBar(_T("mainmenu")));

  CreateStatusBar(1);

  objectBrowser = new ObjectBrowser(this, Pqwx_ObjectBrowser);
  scriptsBook = new wxNotebook(this, Pqwx_ScriptsNotebook);
  resultsBook = new wxNotebook(this, Pqwx_ResultsNotebook);

  resultsPanel = new wxPanel(resultsBook, Pqwx_ResultsPage);
  resultsBook->AddPage(resultsPanel, _("&Results"), true);

  messagesPanel = new wxPanel(resultsBook, Pqwx_MessagesPage);
  resultsBook->AddPage(messagesPanel, _("&Messages"), false);

  planPanel = new wxPanel(resultsBook, Pqwx_PlanPage);
  resultsBook->AddPage(planPanel, _("&Query Plan"), false);

  wxSizer *editorSizer = new wxBoxSizer(wxVERTICAL);
  editorSizer->Add(scriptsBook, 3, wxEXPAND);
  editorSizer->Add(resultsBook, 1, wxEXPAND);

  wxSizer *mainSizer = new wxBoxSizer(wxHORIZONTAL);
  mainSizer->Add(objectBrowser, 1, wxEXPAND);
  mainSizer->Add(editorSizer, 3, wxEXPAND);
  SetSizer(mainSizer);

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
  wxString description(_("PostgreSQL query tool"));
#ifdef PQWX_DEBUG
  description << _(" - Debug build");
  description << _("\nlibpq ") << _T(PG_VERSION);
  description << _T("\n") << wxVERSION_STRING << _T(" ") << _T(WX_FLAVOUR);
#endif
  info.SetDescription(description);
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
  objectBrowser->DisconnectSelected();
}

void PqwxFrame::OnFindObject(wxCommandEvent& event) {
  objectBrowser->FindObject();
}

void PqwxFrame::OnCloseFrame(wxCloseEvent& event) {
  Destroy();
}
