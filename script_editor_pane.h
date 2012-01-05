// -*- mode: c++ -*-

#ifndef __script_editor_pane_h
#define __script_editor_pane_h

#include "database_connection.h"
#include "execution_lexer.h"
#include "database_work.h"
#include "pg_error.h"
#include "documents_notebook.h"

class ScriptEditor;
class ResultsNotebook;
class wxSplitterWindow;

extern "C" void ScriptEditorNoticeReceiver(void *arg, const PGresult *rs);

class ScriptEditorPane : public wxPanel, public ConnectableEditor {
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

  void MarkModified(bool value) { modified = value; UpdateStateInUI(); }

  wxString FormatTitle() const;
private:
  ScriptEditor *editor;
  wxSplitterWindow *splitter;
  ResultsNotebook *resultsBook;
  wxStatusBar *statusbar;

  ServerConnection server;
  DatabaseConnection *db;
  DatabaseConnectionState state;
  wxString coreTitle;
  wxString scriptFilename;
  bool modified;

  void UpdateStateInUI();

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
    UpdateStateInUI();
  }

  friend class ChangeScriptConnection;

  // execution
  wxCharBuffer source;
  ExecutionLexer *lexer;
  wxStopWatch executionTime;
  unsigned rowsRetrieved, errorsEncountered;

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

  static const int StatusBar_Status = 0;
  static const int StatusBar_Server = 1;
  static const int StatusBar_Database = 2;
  static const int StatusBar_TimeElapsed = 3;
  static const int StatusBar_RowsRetrieved = 4;
  static const int StatusBar_TransactionStatus = 5;
  static const int StatusBar_Fields = 6;

  void ShowConnectedStatus();
  void ShowDisconnectedStatus();
  void ShowScriptInProgressStatus();
  void ShowScriptCompleteStatus();

  wxString TxnStatus() const {
    wxASSERT(db != NULL);
    switch (state) {
    case Idle:
    default:
      return wxEmptyString;
    case IdleInTransaction:
      return _("TXN");
    case TransactionAborted:
      return _("ERR");
    case CopyToClient:
    case CopyToServer:
      return _("COPY");
    }
  }

  DECLARE_EVENT_TABLE()
};

#endif
