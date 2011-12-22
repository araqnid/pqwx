// -*- c++ -*-

#ifndef __pqwx_frame_h
#define __pqwx_frame_h

#include "wx/notebook.h"
#include "wx/panel.h"
#include "object_browser.h"

class PqwxFrame: public wxFrame {
public:
  PqwxFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);

  void OnConnectObjectBrowser(wxCommandEvent& event);
  void OnDisconnectObjectBrowser(wxCommandEvent& event);
  void OnFindObject(wxCommandEvent& event);

  void OnCloseFrame(wxCloseEvent& event);

  ObjectBrowser *objectBrowser;
  wxPanel *resultsPanel;
  wxPanel *messagesPanel;
  wxPanel *planPanel;
  wxNotebook *resultsBook;
  wxNotebook *scriptsBook;
private:
  DECLARE_EVENT_TABLE();
};

// controls and menu commands
enum {
  // local
  Pqwx_ObjectBrowser = 16384,
  Pqwx_ConnectObjectBrowser = 16385,
  Pqwx_DisconnectObjectBrowser = 16386,
  Pqwx_MainSplitter = 16387,
  Pqwx_EditorSplitter = 16388,
  Pqwx_ScriptsNotebook = 16389,
  Pqwx_ResultsNotebook = 16390,
  Pqwx_ResultsPage = 16391,
  Pqwx_MessagesPage = 16392,
  Pqwx_PlanPage = 16393,
  Pqwx_ObjectFinderResults,
};

#endif
