// -*- mode: c++ -*-

#ifndef __script_editor_pane_h
#define __script_editor_pane_h

#include "database_connection.h"
#include "execution_lexer.h"
#include "database_work.h"
#include "pg_error.h"

class ScriptEditor;
class ResultsNotebook;
class wxSplitterWindow;

extern "C" void ScriptEditorNoticeReceiver(void *arg, const PGresult *rs);

class ScriptEditorPane : public wxPanel {
public:
  ScriptEditorPane(wxWindow *parent, wxWindowID id = wxID_ANY);
  ~ScriptEditorPane() {
    if (db != NULL) {
      wxLogDebug(_T("Disposing of editor database connection from destructor"));
      db->Dispose();
      delete db;
    }
  }

  void OnDisconnect(wxCommandEvent &event);
  void OnReconnect(wxCommandEvent &event);
  void OnExecute(wxCommandEvent &event);
  void OnQueryComplete(wxCommandEvent &event);
  void OnConnectionNotice(const PGresult *rs);

  ScriptEditor *GetEditor() const { return editor; }
  ResultsNotebook *GetResults() const { return resultsBook; }

  void OpenFile(const wxString &filename);
  void Populate(const wxString &text);
  void Connect(const ServerConnection &server, const wxString &dbname);
  void SetConnection(const ServerConnection &server, DatabaseConnection *db);
  bool HasConnection() const { return db != NULL; }
  bool IsConnected() const { return db != NULL && db->IsConnected(); }
  wxString ConnectionIdentification() const { wxASSERT(db != NULL); return db->Identification(); }
  bool IsExecuting() const { return lexer != NULL; }

  const ServerConnection& GetServer() const { return server; }
  wxString GetDatabase() const { if (db == NULL) return wxEmptyString; else return db->DbName(); }

  void MarkModified(bool value) { modified = value; EmitScriptSelected(); }

  wxString FormatTitle();
private:
  ScriptEditor *editor;
  wxSplitterWindow *splitter;
  ResultsNotebook *resultsBook;

  ServerConnection server;
  DatabaseConnection *db;
  DatabaseConnectionState state;
  wxString coreTitle;
  wxString scriptFilename;
  bool modified;

  void EmitScriptSelected();

  static int documentCounter;

  class SetupNoticeProcessorWork : public DatabaseWork {
  public:
    SetupNoticeProcessorWork(ScriptEditorPane *owner) : owner(owner) {}
    void Execute() {
      PQsetNoticeReceiver(conn, ScriptEditorNoticeReceiver, owner);
    }
    void NotifyFinished() {
    }
  private:
    ScriptEditorPane * const owner;
  };

  class ChangeScriptConnection : public ConnectDialogue::CompletionCallback {
  public:
    ChangeScriptConnection(ScriptEditorPane *owner) : owner(owner) {}
    void Connected(const ServerConnection &server, DatabaseConnection *db)
    {
      owner->SetConnection(server, db);
    }
    void Cancelled()
    {
    }
  private:
    ScriptEditorPane *const owner;
  };

  void UpdateConnectionState(DatabaseConnectionState newState) {
    state = newState;
    EmitScriptSelected();
  }

  friend class ChangeScriptConnection;

  // execution
  wxCharBuffer source;
  ExecutionLexer *lexer;

#ifdef __WXMSW__
  const char *ExtractSQL(const ExecutionLexer::Token &token) const
  {
    char *str = (char*) malloc(token.length + 1);
    memcpy(str, source.data() + token.offset, token.length);
    str[token.length] = '\0';
    return str;
  }
#else
  const char *ExtractSQL(const ExecutionLexer::Token &token) const { return strndup(source.data() + token.offset, token.length); }
#endif

  bool ProcessExecution();
  void FinishExecution();

  ResultsNotebook *GetOrCreateResultsBook() { if (resultsBook == NULL) CreateResultsBook(); return resultsBook; };
  void CreateResultsBook();

  friend class ScriptQueryWork;
  friend class ScriptEditor;

  DECLARE_EVENT_TABLE()
};

#endif
