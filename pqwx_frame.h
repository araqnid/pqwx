// -*- c++ -*-

#ifndef __pqwx_frame_h
#define __pqwx_frame_h

#include "wx/notebook.h"
#include "wx/panel.h"
#include "object_browser.h"
#include "results_notebook.h"
#include "scripts_notebook.h"

class PqwxFrame: public wxFrame {
public:
  PqwxFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);

  void OnConnectObjectBrowser(wxCommandEvent& event);
  void OnDisconnectObjectBrowser(wxCommandEvent& event);
  void OnFindObject(wxCommandEvent& event);
  void OnNewScript(wxCommandEvent& event);
  void OnOpenScript(wxCommandEvent& event);

  void OnCloseFrame(wxCloseEvent& event);

  ObjectBrowser *objectBrowser;
  ResultsNotebook *resultsBook;
  ScriptsNotebook *scriptsBook;
private:
  DECLARE_EVENT_TABLE();
};

#endif
