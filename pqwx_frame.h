/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __pqwx_frame_h
#define __pqwx_frame_h

#include "wx/notebook.h"
#include "wx/panel.h"
#include "wx/splitter.h"
#include "object_browser.h"
#include "documents_notebook.h"
#include "connect_dialogue.h"

/**
 * PQWX application frame.
 */
class PqwxFrame: public wxFrame {
public:
  /**
   * Create frame, specifying initial title.
   */
  PqwxFrame(const wxString& title);

  /**
   * @return object browser in this frame
   */
  ObjectBrowser *GetObjectBrowser() const { return objectBrowser; }

  /**
   * Open script and connect it to the given database.
   */
  void OpenScript(const wxString &filename, const ServerConnection &server, const wxString &database);

private:
  /**
   * Connection callback to add a connection to the object browser once it is established.
   */
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

  wxString titlePrefix;

  ObjectBrowser *objectBrowser;
  DocumentsNotebook *documentsBook;

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
  void OnCloseScript(wxCommandEvent& event);
  void OnSaveScript(wxCommandEvent& event);
  void OnSaveScriptAs(wxCommandEvent& event);
  void OnScriptToWindow(PQWXDatabaseEvent& event);
  void OnDocumentSelected(PQWXDatabaseEvent& event);
  void OnNoDocumentSelected(wxCommandEvent& event);
  void OnObjectSelected(PQWXDatabaseEvent& event);
  void OnNoObjectSelected(wxCommandEvent& event);
  void OnScriptNew(PQWXDatabaseEvent& event);

  void OnCloseFrame(wxCloseEvent& event);

  void EnableIffHaveObjectBrowserDatabase(wxUpdateUIEvent& event);
  void EnableIffHaveObjectBrowserServer(wxUpdateUIEvent& event);
  void EnableIffScriptOpen(wxUpdateUIEvent& event);
  void EnableIffScriptConnected(wxUpdateUIEvent& event);
  void EnableIffScriptIdle(wxUpdateUIEvent& event);
  void EnableIffScriptModified(wxUpdateUIEvent& event);

  ConnectableEditor *currentEditor;
  wxEvtHandler *currentEditorTarget;
  
  bool haveCurrentServer;
  ServerConnection currentServer;
  wxString currentDatabase;

  wxStopWatch scriptExecutionStopwatch;

  void SaveFrameGeometry() const;
  void LoadFrameGeometry();

  friend class AddConnectionToObjectBrowser;

  DECLARE_EVENT_TABLE();
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
