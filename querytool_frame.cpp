#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "querytool_frame.h"

#if !defined(__WXMSW__) && !defined(__WXPM__)
    #include "querytool-appicon.xpm"
#endif

BEGIN_EVENT_TABLE(QueryToolFrame, wxFrame)
  EVT_MENU(QueryTool_Quit, QueryToolFrame::OnQuit)
  EVT_MENU(QueryTool_About, QueryToolFrame::OnAbout)
END_EVENT_TABLE()

QueryToolFrame::QueryToolFrame(const wxString& title)
  : wxFrame(NULL, wxID_ANY, title)
{
  SetIcon(wxICON(querytool_appicon));

  wxMenu *fileMenu = new wxMenu;
  fileMenu->Append(QueryTool_Quit, _T("E&xit\tCtrl-Q"), _T("Exit Query Tool"));

  wxMenu *helpMenu = new wxMenu;
  helpMenu->Append(QueryTool_About, _T("&About...\tF1"), _T("Show information about Query Tool"));

  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(fileMenu, _T("&File"));
  menuBar->Append(helpMenu, _T("&Help"));

  SetMenuBar(menuBar);

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
