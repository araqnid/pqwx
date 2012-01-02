// -*- mode: c++ -*-

#ifndef __script_editor_h
#define __script_editor_h

#include "wx/stc/stc.h"
#include "database_connection.h"
#include "execution_lexer.h"
#include "database_work.h"
#include "pg_error.h"
#include "connect_dialogue.h"

class ScriptModel;
class ScriptsNotebook;
class ScriptQueryWork;

extern "C" void ScriptEditorNoticeReceiver(void *arg, const PGresult *rs);

class ScriptEditor : public wxStyledTextCtrl {
public:
  ScriptEditor(ScriptsNotebook *owner, wxWindowID id);
  ~ScriptEditor() {
    if (db != NULL) {
      wxLogDebug(_T("Disposing of editor database connection from destructor"));
      db->Dispose();
      delete db;
    }
  }

  void OnSetFocus(wxFocusEvent &event);
  void OnLoseFocus(wxFocusEvent &event);
  void OnSavePointLeft(wxStyledTextEvent &event);
  void OnSavePointReached(wxStyledTextEvent &event);
  void OnExecute(wxCommandEvent &event);
  void OnDisconnect(wxCommandEvent &event);
  void OnReconnect(wxCommandEvent &event);
  void OnQueryComplete(wxCommandEvent &event);
  void OnConnectionNotice(const PGresult *rs);

  void Connect(const ServerConnection &server, const wxString &dbname);
  void SetConnection(const ServerConnection &server, DatabaseConnection *db);
  bool HasConnection() const { return db != NULL; }
  bool IsConnected() const { return db != NULL && db->IsConnected(); }
  wxString ConnectionIdentification() const { wxASSERT(db != NULL); return db->Identification(); }
  bool IsExecuting() const { return lexer != NULL; }

private:
  ScriptsNotebook *owner;
  ServerConnection server;
  DatabaseConnection *db;

  void EmitScriptSelected();

  // execution
  wxStopWatch executionStopwatch;
  wxCharBuffer source;
  ExecutionLexer *lexer;

  bool ProcessExecution();
  void FinishExecution();
  const char *ExtractSQL(const ExecutionLexer::Token &token) const { return strndup(source.data() + token.offset, token.length); }

  friend class ScriptQueryWork;

  class SetupNoticeProcessorWork : public DatabaseWork {
  public:
    SetupNoticeProcessorWork(ScriptEditor *editor) : editor(editor) {}
    void Execute() {
      PQsetNoticeReceiver(conn, ScriptEditorNoticeReceiver, editor);
    }
    void NotifyFinished() {
    }
  private:
    ScriptEditor * const editor;
  };

  class ChangeScriptConnection : public ConnectDialogue::CompletionCallback {
  public:
    ChangeScriptConnection(ScriptEditor *owner) : owner(owner) {}
    void Connected(const ServerConnection &server, DatabaseConnection *db)
    {
      owner->SetConnection(server, db);
    }
    void Cancelled()
    {
    }
  private:
    ScriptEditor *const owner;
  };

  friend class ChangeScriptConnection;

  DECLARE_EVENT_TABLE()
};

#endif
