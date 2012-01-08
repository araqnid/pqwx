/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

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

/**
 * Function to accept asynchronous notices from libpq and pass them to the editor pane.
 */
extern "C" void ScriptEditorNoticeReceiver(void *arg, const PGresult *rs);

/**
 * Script editor, with results notebook and status bar.
 *
 * This also functions as the model of the script editor. It contains
 * the database connection and methods to execute the script in the
 * editor buffer.
 */
class ScriptEditorPane : public wxPanel, public ConnectableEditor {
public:
  /**
   * Create editor pane.
   */
  ScriptEditorPane(wxWindow *parent, wxWindowID id = wxID_ANY);
  ~ScriptEditorPane()
  {
    if (execution != NULL) {
#ifdef __WXDEBUG__
      wxLogError(_T("Disposing of script execution from destructor"));
#endif
      delete execution;
    }
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
  void OnTimerTick(wxTimerEvent &event);

  /**
   * Gets the script editor.
   */
  ScriptEditor *GetEditor() const { return editor; }
  /**
   * Gets the results notebook.
   */
  ResultsNotebook *GetResults() const { return resultsBook; }

  /**
   * Open a file in the editor.
   */
  void OpenFile(const wxString &filename);
  /**
   * Save the to the current file.
   */
  void SaveFile() { wxASSERT(!scriptFilename.empty()); SaveFile(scriptFilename); }
  /**
   * Save to a specified file.
   */
  void SaveFile(const wxString &filename);
  /**
   * Populate the editor with the given text.
   */
  void Populate(const wxString &text);
  /**
   * Connect to the specified database.
   */
  void Connect(const ServerConnection &server, const wxString &dbname);
  /**
   * Register an existing database connection.
   */
  void SetConnection(const ServerConnection &server, DatabaseConnection *db);
  /**
   * @return true if this editor has a connection
   */
  bool HasConnection() const { return db != NULL; }
  /**
   * @return true if this editor has a connection and is currently connected
   */
  bool IsConnected() const { return db != NULL && db->IsConnected(); }
  /**
   * @return string identifying the database connection
   */
  wxString ConnectionIdentification() const { wxASSERT(db != NULL); return db->Identification(); }
  /**
   * @return true if the script is currently being executed
   */
  bool IsExecuting() const { return execution != NULL; }

  /**
   * @return server connection properties
   */
  const ServerConnection& GetServer() const { return server; }
  /**
   * @return database name
   */
  wxString GetDatabase() const { if (db == NULL) return wxEmptyString; else return db->DbName(); }
  /**
   * @return current state of database connection
   */
  DatabaseConnectionState GetConnectionState() const { return state; }

  /**
   * @return true if editor content modified
   */
  bool IsModified() const { return modified; }
  /**
   * Mark editor content as modified
   * @param value true to mark as modfied, false to mark as unmodified.
   */
  void MarkModified(bool value) { modified = value; UpdateStateInUI(); }
  /**
   * @return true if editor has a full filename associated with it
   */
  bool HasFilename() const { return !scriptFilename.empty(); }
  /**
   * @return leafname of file if one is set, otherwise a dummy filename ("Query-1.sql" etc)
   */
  wxString GetCoreTitle() const { return coreTitle; }

  /**
   * @return formatted editor title, with database identification and is-modified flag
   */
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
    void Execute()
    {
      PQsetNoticeReceiver(conn, ScriptEditorNoticeReceiver, owner);
    }
    void NotifyFinished()
    {
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

  class Execution {
  public:
    Execution(wxCharBuffer buffer, unsigned length) :
      buffer(buffer),
      lexer(this->buffer.data(), length),
      lastSqlToken(ExecutionLexer::Token::END), lastSqlExecuted(false),
      rowsRetrieved(0), errorsEncountered(0)
    {
      stopwatch.Start();
    }

#ifdef __WXMSW__
    const char *ExtractSQL(const ExecutionLexer::Token &token) const
    {
      char *str = (char*) malloc(token.length + 1);
      memcpy(str, buffer.data() + token.offset, token.length);
      str[token.length] = '\0';
      return str;
    }
#else
    const char *ExtractSQL(const ExecutionLexer::Token &token) const { return strndup(buffer.data() + token.offset, token.length); }
#endif

    char CharAt(unsigned offset) const { return buffer.data()[offset]; }
    bool EncounteredErrors() const { return errorsEncountered > 0; }
    unsigned TotalRows() const { return rowsRetrieved; }
    long ElapsedTime() const { return stopwatch.Time(); }
    void AddRows(unsigned rows) { rowsRetrieved += rows; }
    void BumpErrors() { ++errorsEncountered; }

    ExecutionLexer::Token NextToken() { return lexer.Pull(); }
    ExecutionLexer::Token CopyDataToken() { return lexer.ReadCopyData(); }
    void SetLastSqlToken(ExecutionLexer::Token t) { lastSqlToken = t; lastSqlExecuted = false; }
    const ExecutionLexer::Token& GetLastSqlToken() const { return lastSqlToken; }
    ExecutionLexer::Token PopLastSqlToken() { ExecutionLexer::Token t = lastSqlToken; ClearLastSqlToken(); return t; }
    void ClearLastSqlToken() { lastSqlToken = ExecutionLexer::Token(ExecutionLexer::Token::END); }
    bool LastSqlTokenValid() const { return lastSqlToken.type == ExecutionLexer::Token::SQL; }
    bool SqlPending() const { return LastSqlTokenValid() && !lastSqlExecuted; }
    void MarkSqlExecuted() { lastSqlExecuted = true; }

    wxString GetWXString(const ExecutionLexer::Token &token) const { return lexer.GetWXString(token); }
    const char* GetBuffer() const { return buffer.data(); }
  private:
    wxCharBuffer buffer;
    ExecutionLexer lexer;
    ExecutionLexer::Token lastSqlToken;
    bool lastSqlExecuted;
    unsigned rowsRetrieved, errorsEncountered;
    wxStopWatch stopwatch;
  };

  Execution *execution;
  wxTimer statusUpdateTimer;

  bool ProcessExecution();
  void BeginQuery(ExecutionLexer::Token t);
  void BeginPutCopyData();
  void FinishExecution();

  typedef bool (ScriptEditorPane::*PsqlCommandHandler)(const wxString&);
  static const std::map<wxString, PsqlCommandHandler> psqlCommandHandlers;
  static std::map<wxString, PsqlCommandHandler> InitPsqlCommandHandlers();

  bool PsqlChangeDatabase(const wxString &args);
  bool PsqlExecuteBuffer(const wxString &args);
  bool PsqlPrintMessage(const wxString &args);

  void ReportInternalError(const wxString &error, const wxString &command);

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

  wxString TxnStatus() const
  {
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

// Local Variables:
// mode: c++
// End:
