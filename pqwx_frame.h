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
  void OnExecuteScript(wxCommandEvent& event);
  void OnNewScript(wxCommandEvent& event);
  void OnOpenScript(wxCommandEvent& event);
  void OnScriptToWindow(wxCommandEvent& event);
  void OnScriptSelected(wxCommandEvent& event);
  void OnObjectSelected(wxCommandEvent& event);

  void OnCloseFrame(wxCloseEvent& event);

private:
  ObjectBrowser *objectBrowser;
  ResultsNotebook *resultsBook;
  ScriptsNotebook *scriptsBook;
  ScriptEditor *currentEditor;

  friend class PQWXApp;

  DECLARE_EVENT_TABLE();
};

#endif
