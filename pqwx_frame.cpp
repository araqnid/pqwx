#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/toolbar.h"

#include "pqwx_frame.h"
#include "object_browser.h"

#if !defined(__WXMSW__) && !defined(__WXPM__)
    #include "pqwx-appicon.xpm"
#endif

#include "toolbar-new.xpm"

BEGIN_EVENT_TABLE(PqwxFrame, wxFrame)
  EVT_MENU(Pqwx_Quit, PqwxFrame::OnQuit)
  EVT_MENU(Pqwx_About, PqwxFrame::OnAbout)
  EVT_MENU(Pqwx_New, PqwxFrame::OnNew)
  EVT_MENU(Pqwx_Open, PqwxFrame::OnOpen)
  EVT_CLOSE(PqwxFrame::OnCloseFrame)
END_EVENT_TABLE()

const int TOOLBAR_MAIN = 500;

PqwxFrame::PqwxFrame(const wxString& title)
  : wxFrame(NULL, wxID_ANY, title)
{
  SetIcon(wxICON(Pqwx_appicon));

  wxMenu *fileMenu = new wxMenu;
  fileMenu->Append(Pqwx_New, _T("&New query\tCtrl-N"), _T("Create a new query"));
  fileMenu->Append(Pqwx_Open, _T("&Open script\tCtrl-O"), _T("Open a SQL script"));
  fileMenu->Append(Pqwx_Quit, _T("E&xit\tCtrl-Q"), _T("Exit PQWX"));

  wxMenu *helpMenu = new wxMenu;
  helpMenu->Append(Pqwx_About, _T("&About...\tF1"), _T("Show information about PQWX"));

  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(fileMenu, _T("&File"));
  menuBar->Append(helpMenu, _T("&Help"));

  SetMenuBar(menuBar);

  CreateToolBar(wxNO_BORDER | wxHORIZONTAL | wxTB_FLAT, TOOLBAR_MAIN);
  GetToolBar()->SetMargins(2, 2);
  GetToolBar()->SetToolBitmapSize(wxSize(16, 15));
  GetToolBar()->AddTool(wxID_NEW, _T("NEW"), wxBitmap(new_xpm), wxNullBitmap, wxITEM_NORMAL,
			_T("New query"), _T("Create a new query"));
  GetToolBar()->Realize();

  CreateStatusBar(2);

  objectBrowser = new ObjectBrowser(this, Pqwx_ObjectBrowser);
}

void PqwxFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
  Close(true);
}

void PqwxFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
  wxMessageBox(wxString::Format(_T("Welcome to PQWX version %s\n")
				_T("\n")
#ifdef PQWX_DEBUG
				_T("This is a debug build.\n")
#endif
				_T("Using %s\n") _T("Running on %s."),
				_T(PQWX_VERSION),
				wxVERSION_STRING,
				wxGetOsDescription().c_str()
				),
	       _T("About PQWX"),
	       wxOK | wxICON_INFORMATION,
	       this
	       );
}

void PqwxFrame::OnNew(wxCommandEvent& event)
{
  wxLogStatus(_T("Create new script here..."));
}

void PqwxFrame::OnOpen(wxCommandEvent& event)
{
  wxFileDialog *openFileDialog = new wxFileDialog(this);

  openFileDialog->SetWildcard(_T("SQL files (*.sql)|*.sql"));

  if (openFileDialog->ShowModal() == wxID_OK) {
    wxString filename = openFileDialog->GetPath();
    wxLogStatus(_T("Load file %s..."), filename.c_str());
  }
}

void PqwxFrame::AddServerConnection(ServerConnection *conn) {
  objectBrowser->AddServerConnection(conn);
}

void PqwxFrame::OnCloseFrame(wxCloseEvent& event) {
  objectBrowser->dispose();
  Destroy();
}
