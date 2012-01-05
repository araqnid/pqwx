// -*- c++ -*-

#ifndef __pqwx_frame_h
#define __pqwx_frame_h

#include "wx/notebook.h"
#include "wx/panel.h"
#include "wx/splitter.h"
#include "object_browser.h"
#include "documents_notebook.h"
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
  void OnDocumentSelected(wxCommandEvent& event);
  void OnObjectSelected(PQWXDatabaseEvent& event);
  void OnScriptNew(PQWXDatabaseEvent& event);

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
  DocumentsNotebook *documentsBook;
  ScriptEditorPane *currentEditor;

  bool haveCurrentServer;
  ServerConnection currentServer;
  wxString currentDatabase;

  wxStopWatch scriptExecutionStopwatch;

  void UpdateStatusBar(const ServerConnection &server, const wxString &database);

  void SaveFrameGeometry() const;
  void LoadFrameGeometry();

  friend class AddConnectionToObjectBrowser;

  static const int StatusBar_Message = 0;
  static const int StatusBar_Server = 1;
  static const int StatusBar_Database = 2;
  static const int StatusBar_State = 3;
  static const int StatusBar_Fields = 4;

  DECLARE_EVENT_TABLE();
};

#endif
