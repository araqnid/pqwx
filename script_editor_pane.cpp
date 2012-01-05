#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/splitter.h"
#include "script_editor.h"
#include "script_editor_pane.h"
#include "script_events.h"
#include "results_notebook.h"
#include "database_work.h"
#include "script_query_work.h"

BEGIN_EVENT_TABLE(ScriptEditorPane, wxPanel)
  PQWX_SCRIPT_EXECUTE(wxID_ANY, ScriptEditorPane::OnExecute)
  PQWX_SCRIPT_DISCONNECT(wxID_ANY, ScriptEditorPane::OnDisconnect)
  PQWX_SCRIPT_RECONNECT(wxID_ANY, ScriptEditorPane::OnReconnect)
  PQWX_SCRIPT_QUERY_COMPLETE(wxID_ANY, ScriptEditorPane::OnQueryComplete)
END_EVENT_TABLE()

int ScriptEditorPane::documentCounter = 0;

ScriptEditorPane::ScriptEditorPane(wxWindow *parent, wxWindowID id)
  : wxPanel(parent, id), resultsBook(NULL), db(NULL), modified(false), lexer(NULL)
{
  splitter = new wxSplitterWindow(this, wxID_ANY);
  editor = new ScriptEditor(splitter, wxID_ANY, this);
  statusbar = new wxStatusBar(this, wxID_ANY, 0L);

  wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(splitter, 1, wxEXPAND);
  sizer->Add(statusbar, 0, wxEXPAND);
  SetSizer(sizer);

  splitter->Initialize(editor);
  splitter->SetMinimumPaneSize(100);
  splitter->SetSashGravity(1.0);
 
  coreTitle = wxString::Format(_("Query-%d.sql"), ++documentCounter);
}

void ScriptEditorPane::CreateResultsBook()
{
  resultsBook = new ResultsNotebook(splitter, wxID_ANY);
  splitter->SplitHorizontally(editor, resultsBook);
}

void ScriptEditorPane::OpenFile(const wxString &filename)
{
  wxString tabName;
#ifdef __WXMSW__
  static const wxChar PathSeparator = _T('\\');
#else
  static const wxChar PathSeparator = _T('/');
#endif

  size_t slash = filename.find_last_of(PathSeparator);
  if (slash == wxString::npos)
    coreTitle = filename;
  else
    coreTitle = filename.Mid(slash + 1);

  editor->LoadFile(filename);
  UpdateStateInUI();
}

void ScriptEditorPane::Populate(const wxString &text) {
  editor->AddText(text);
  editor->EmptyUndoBuffer();
}

void ScriptEditorPane::Connect(const ServerConnection &server_, const wxString &dbname)
{
  wxASSERT(db == NULL);
  server = server_;
  db = new DatabaseConnection(server, dbname.empty() ? server.globalDbName : dbname, _("Query"));
  // TODO handle connection problems, direct through connection dialogue
  db->Connect();
  state = Idle;
  ShowConnectedStatus();
  db->AddWork(new SetupNoticeProcessorWork(this));
  UpdateStateInUI();
}

void ScriptEditorPane::SetConnection(const ServerConnection &server_, DatabaseConnection *db_)
{
  if (db != NULL) {
    db->Dispose();
    delete db;
  }

  db = db_;
  server = server_;
  db->Relabel(_("Query"));
  db->AddWork(new SetupNoticeProcessorWork(this));
  state = Idle;
  ShowConnectedStatus();
  UpdateStateInUI();
}

void ScriptEditorPane::OnDisconnect(wxCommandEvent &event)
{
  wxASSERT(lexer == NULL); // must not be executing
  wxASSERT(db != NULL);
  db->Dispose();
  delete db;
  db = NULL;
  ShowDisconnectedStatus();
  UpdateStateInUI(); // refresh title etc
}

void ScriptEditorPane::OnReconnect(wxCommandEvent &event)
{
  wxASSERT(lexer == NULL); // must not be executing

  ConnectDialogue *dbox = new ConnectDialogue(NULL, new ChangeScriptConnection(this));
  if (db != NULL)
    dbox->Suggest(server);
  dbox->Show();
  dbox->SetFocus();
}

void ScriptEditorNoticeReceiver(void *arg, const PGresult *rs)
{
  // FIXME needs to be an event
  ScriptEditorPane *editor = (ScriptEditorPane*) arg;
  editor->OnConnectionNotice(rs);
}

void ScriptEditorPane::OnConnectionNotice(const PGresult *rs)
{
  wxASSERT(PQresultStatus(rs) == PGRES_NONFATAL_ERROR);
  PgError error(rs);

  GetOrCreateResultsBook()->ScriptNotice(error);
}

wxString ScriptEditorPane::FormatTitle() const {
  wxString output;
  if (db == NULL)
    output << _("<disconnected>");
  else
    output << db->Identification();
  output << _T(" - ") << coreTitle;
  if (modified) output << _T(" *");
  return output;
}

void ScriptEditorPane::UpdateStateInUI()
{
  wxString dbname;
  if (db) dbname = db->DbName();
  PQWXDatabaseEvent event(server, dbname, PQWX_ScriptStateUpdated);
  event.SetString(FormatTitle());
  event.SetEventObject(this);
  if (db) event.SetConnectionState(state);
  ProcessEvent(event);
}

void ScriptEditorPane::ShowConnectedStatus()
{
  statusbar->SetFieldsCount(StatusBar_Fields);
  static const int StatusBar_Widths[] = { -1, 120, 80, 80, 80, 40 };
  statusbar->SetStatusWidths(sizeof(StatusBar_Widths)/sizeof(int), StatusBar_Widths);
  statusbar->SetStatusText(_("Connected."), StatusBar_Status);
  statusbar->SetStatusText(server.Identification(), StatusBar_Server);
  statusbar->SetStatusText(db->DbName(), StatusBar_Database);
  statusbar->SetStatusText(TxnStatus(), StatusBar_TransactionStatus);
}

void ScriptEditorPane::ShowDisconnectedStatus()
{
  statusbar->SetFieldsCount(1);
  statusbar->SetStatusText(_("Diconnected."), StatusBar_Status);
}

void ScriptEditorPane::ShowScriptInProgressStatus()
{
  statusbar->SetStatusText(_("Executing query..."), StatusBar_Status);
  statusbar->SetStatusText(_T("00:00"), StatusBar_TimeElapsed);
  statusbar->SetStatusText(wxEmptyString, StatusBar_RowsRetrieved);
}

void ScriptEditorPane::ShowScriptCompleteStatus()
{
  if (errorsEncountered == 0)
    statusbar->SetStatusText(_("Query completed successfully"), StatusBar_Status);
  else
    statusbar->SetStatusText(_("Query completed with errors"), StatusBar_Status);
  statusbar->SetStatusText(_T("??"), StatusBar_TimeElapsed);
  statusbar->SetStatusText(wxString::Format(_("%d rows"), rowsRetrieved), StatusBar_RowsRetrieved);
}

void ScriptEditorPane::OnExecute(wxCommandEvent &event)
{
  wxASSERT(db != NULL);
  wxASSERT(lexer == NULL);

  wxCommandEvent beginEvt = wxCommandEvent(PQWX_ScriptExecutionBeginning);
  beginEvt.SetEventObject(this);
  ProcessEvent(beginEvt);
  executionTime.Start();
  rowsRetrieved = 0;
  errorsEncountered = 0;
  ShowScriptInProgressStatus();

  if (resultsBook != NULL) resultsBook->Reset();

  // wxCharBuffer works like auto_ptr, so can't copy it.
  int length;
  source = editor->GetRegion(&length);
  lexer = new ExecutionLexer(source.data(), length);

  while (true) {
    if (!ProcessExecution()) {
      break;
    }
  }
}

bool ScriptEditorPane::ProcessExecution()
{
  ExecutionLexer::Token t = lexer->Pull();
  if (t.type == ExecutionLexer::Token::END) {
    FinishExecution();
    return false;
  }

  if (t.type == ExecutionLexer::Token::SQL) {
    bool added = db->AddWorkOnlyIfConnected(new ScriptQueryWork(this, t, ExtractSQL(t)));
    wxASSERT(added);
    return false;
  }
  else {
    wxLogDebug(_T("psql | %s"), lexer->GetWXString(t).c_str());
    return true;
  }
}

void ScriptEditorPane::FinishExecution()
{
  wxCommandEvent finishEvt = wxCommandEvent(PQWX_ScriptExecutionFinishing);
  finishEvt.SetEventObject(this);
  ProcessEvent(finishEvt);

  delete lexer;
  lexer = NULL;

  ShowScriptCompleteStatus();
}

void ScriptEditorPane::OnQueryComplete(wxCommandEvent &event)
{
  ScriptQueryWork::Result *result = (ScriptQueryWork::Result*) event.GetClientData();
  wxASSERT(result != NULL);

  if (result->status == PGRES_TUPLES_OK) {
    wxLogDebug(_T("%s (%lu tuples)"), result->statusTag.c_str(), result->data->size());
    GetOrCreateResultsBook()->ScriptResultSet(result->statusTag, *result->data);
    rowsRetrieved += result->data->size();
  }
  else if (result->status == PGRES_COMMAND_OK) {
    wxLogDebug(_T("%s (no tuples)"), result->statusTag.c_str());
    GetOrCreateResultsBook()->ScriptCommandCompleted(result->statusTag);
  }
  else if (result->status == PGRES_FATAL_ERROR) {
    wxLogDebug(_T("Got error: %s"), result->error.primary.c_str());
    GetOrCreateResultsBook()->ScriptError(result->error);
    errorsEncountered++;
  }
  else {
    wxLogDebug(_T("Got something else: %s"), wxString(PQresStatus(result->status), wxConvUTF8).c_str());
  }

  UpdateConnectionState(result->newConnectionState);

  delete result;

  wxASSERT(lexer != NULL);

  while (true) {
    if (!ProcessExecution()) {
      break;
    }
  }
}

