#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/toolbar.h"

#include "querytool_frame.h"

#if !defined(__WXMSW__) && !defined(__WXPM__)
    #include "querytool-appicon.xpm"
#endif

#include "toolbar-new.xpm"

BEGIN_EVENT_TABLE(QueryToolFrame, wxFrame)
  EVT_MENU(QueryTool_Quit, QueryToolFrame::OnQuit)
  EVT_MENU(QueryTool_About, QueryToolFrame::OnAbout)
  EVT_MENU(QueryTool_New, QueryToolFrame::OnNew)
  EVT_MENU(QueryTool_Open, QueryToolFrame::OnOpen)
END_EVENT_TABLE()

const int TOOLBAR_MAIN = 500;

QueryToolFrame::QueryToolFrame(const wxString& title)
  : wxFrame(NULL, wxID_ANY, title)
{
  SetIcon(wxICON(querytool_appicon));

  wxMenu *fileMenu = new wxMenu;
  fileMenu->Append(QueryTool_New, _T("&New query\tCtrl-N"), _T("Create a new query"));
  fileMenu->Append(QueryTool_Open, _T("&Open script\tCtrl-O"), _T("Open a SQL script"));
  fileMenu->Append(QueryTool_Quit, _T("E&xit\tCtrl-Q"), _T("Exit Query Tool"));

  wxMenu *helpMenu = new wxMenu;
  helpMenu->Append(QueryTool_About, _T("&About...\tF1"), _T("Show information about Query Tool"));

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
}

void QueryToolFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
  Close(true);
}

void QueryToolFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
  wxMessageBox(wxString::Format(_T("Welcome to Query Tool version %s\n")
				_T("\n")
				_T("Using %s\n") _T("Running on %s."),
				_T("0.1"),
				wxVERSION_STRING,
				wxGetOsDescription().c_str()
				),
	       _T("About Query Tool"),
	       wxOK | wxICON_INFORMATION,
	       this
	       );
}

void QueryToolFrame::OnNew(wxCommandEvent& event)
{
  wxLogStatus(_T("Create new script here..."));
}

void QueryToolFrame::OnOpen(wxCommandEvent& event)
{
  wxLogStatus(_T("Open script file here..."));
}
