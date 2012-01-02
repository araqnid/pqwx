// -*- c++ -*-

#ifndef __pqwx_frame_h
#define __pqwx_frame_h

#include "wx/notebook.h"
#include "wx/panel.h"
#include "object_browser.h"
#include "results_notebook.h"
#include "scripts_notebook.h"
#include "connect_dialogue.h"

class PqwxFrame: public wxFrame {
public:
  PqwxFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);

  void OnConnectObjectBrowser(wxCommandEvent& event);
  void OnDisconnectObjectBrowser(wxCommandEvent& event);
  void OnFindObject(wxCommandEvent& event);
  void OnExecuteScript(wxCommandEvent& event);
  void OnDisconnectScript(wxCommandEvent& event);
  void OnReconnectScript(wxCommandEvent& event);
  void OnNewScript(wxCommandEvent& event);
  void OnOpenScript(wxCommandEvent& event);
  void OnScriptToWindow(PQWXDatabaseEvent& event);
  void OnScriptSelected(PQWXDatabaseEvent& event);
  void OnObjectSelected(PQWXDatabaseEvent& event);
  void OnScriptExecutionBeginning(wxCommandEvent& event);
  void OnScriptExecutionFinishing(wxCommandEvent& event);

  void OnCloseFrame(wxCloseEvent& event);

  class AddConnectionToObjectBrowser : public ConnectDialogue::CompletionCallback {
  public:
    AddConnectionToObjectBrowser(PqwxFrame *owner) : owner(owner) {}
    void Connected(const ServerConnection &server, DatabaseConnection *db) {
      owner->objectBrowser->AddServerConnection(server, db);
    }
    void Cancelled() {
    }
  private:
    PqwxFrame *const owner;
  };

private:
  ObjectBrowser *objectBrowser;
  ResultsNotebook *resultsBook;
  ScriptsNotebook *scriptsBook;
  ScriptEditor *currentEditor;
  wxSizer *editorSizer;
  wxSizer *mainSizer;

  bool haveCurrentServer;
  ServerConnection currentServer;
  wxString currentDatabase;

  wxStopWatch scriptExecutionStopwatch;

  friend class AddConnectionToObjectBrowser;

  DECLARE_EVENT_TABLE();
};

#endif
