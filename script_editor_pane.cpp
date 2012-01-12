#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/splitter.h"
#include "wx/tokenzr.h"
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
  PQWX_SCRIPT_EXECUTION_FINISHING(wxID_ANY, ScriptEditorPane::OnExecutionFinished)
  PQWX_SCRIPT_SERVER_NOTICE(wxID_ANY, ScriptEditorPane::OnConnectionNotice)
  PQWX_SCRIPT_ASYNC_NOTIFICATION(wxID_ANY, ScriptEditorPane::OnConnectionNotification)
  EVT_TIMER(wxID_ANY, ScriptEditorPane::OnTimerTick)
  PQWX_SCRIPT_SHOW_POSITION(wxID_ANY, ScriptEditorPane::OnShowPosition)
END_EVENT_TABLE()

DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptStateUpdated)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptExecutionBeginning)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptExecutionFinishing)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptQueryComplete)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptConnectionStatus)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptServerNotice)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptAsyncNotification)

int ScriptEditorPane::documentCounter = 0;

ScriptEditorPane::ScriptEditorPane(wxWindow *parent, wxWindowID id)
  : wxPanel(parent, id), resultsBook(NULL), db(NULL), modified(false),
    execution(NULL), statusUpdateTimer(this)
#ifdef PQWX_NOTIFICATION_MONITOR
, notificationReceiver(this)
#endif
{
  splitter = new wxSplitterWindow(this, wxID_ANY);
  editor = new ScriptEditor(splitter, wxID_ANY, this);
  statusbar = new wxStatusBar(this, wxID_ANY);

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

  scriptFilename = filename;
  editor->LoadFile(filename);
  UpdateStateInUI();
}

void ScriptEditorPane::SaveFile(const wxString &filename)
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

  scriptFilename = filename;
  editor->WriteFile(filename);
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
  ConnectDialogue dbox(this);
  dbox.DoInitialConnection(server, dbname.empty() ? server.globalDbName : dbname);
  if (dbox.ShowModal() == wxID_CANCEL)
    return;
  db = dbox.GetConnection();
  db->Relabel(_T("Query"));
  state = Idle;
  ShowConnectedStatus();
  db->AddWork(new SetupNoticeProcessorWork(this));
#ifdef PQWX_NOTIFICATION_MONITOR
  db->SetNotificationMonitor(PQWXApp::GetNotificationMonitor(), &notificationReceiver);
#endif
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
#ifdef PQWX_NOTIFICATION_MONITOR
  db->SetNotificationMonitor(PQWXApp::GetNotificationMonitor(), &notificationReceiver);
#endif
  state = Idle;
  ShowConnectedStatus();
  UpdateStateInUI();
}

void ScriptEditorPane::BeginDisconnect()
{
  if (db == NULL) return;
  wxASSERT(execution == NULL); // must not be executing
  db->BeginDisconnection();
}

void ScriptEditorPane::OnDisconnect(wxCommandEvent &event)
{
  wxASSERT(execution == NULL); // must not be executing
  wxASSERT(db != NULL);
  db->Dispose();
  delete db;
  db = NULL;
  ShowDisconnectedStatus();
  UpdateStateInUI(); // refresh title etc
}

void ScriptEditorPane::Dispose()
{
  if (execution != NULL) {
    // well, this isn't handled...
    delete execution;
    execution = NULL;
  }
  if (db != NULL) {
    db->Dispose();
    delete db;
    db = NULL;
  }
}

void ScriptEditorPane::OnReconnect(wxCommandEvent &event)
{
  wxASSERT(execution == NULL); // must not be executing

  ConnectDialogue dbox(this);
  if (db != NULL)
    dbox.Suggest(server);
  if (dbox.ShowModal() == wxID_CANCEL) return;
  SetConnection(server, dbox.GetConnection());
}

void ScriptEditorNoticeReceiver(void *arg, const PGresult *rs)
{
  wxASSERT(PQresultStatus(rs) == PGRES_NONFATAL_ERROR);
  ScriptEditorPane *editor = (ScriptEditorPane*) arg;
  wxCommandEvent event(PQWX_ScriptServerNotice);
  event.SetClientData(new PgError(rs));
  editor->AddPendingEvent(event);
}

void ScriptEditorPane::OnConnectionNotice(wxCommandEvent &event)
{
  PgError *error = (PgError*) event.GetClientData();
  wxASSERT(error != NULL);
  if (execution == NULL)
    GetOrCreateResultsBook()->ScriptAsynchronousNotice(*error);
  else
    execution->ProcessConnectionNotice(*error);
  delete error;
}

#ifdef PQWX_NOTIFICATION_MONITOR
void ScriptEditorPane::MonitorInputProcessor::operator()(PGnotify *notification)
{
  wxString channel(wxString(notification->relname, wxConvUTF8));
  wxString payload(wxString(notification->extra, wxConvUTF8));
  wxCommandEvent event(PQWX_ScriptAsyncNotification);
  if (payload)
    event.SetString(channel + _T(" ") + payload);
  else
    event.SetString(channel);
  owner->AddPendingEvent(event);
}
#endif

void ScriptEditorPane::OnConnectionNotification(wxCommandEvent &event)
{
  GetOrCreateResultsBook()->ScriptAsynchronousNotification(event.GetString());
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
  statusbar->SetStatusText(_("Disconnected."), StatusBar_Status);
}

void ScriptEditorPane::ShowScriptInProgressStatus()
{
  statusbar->SetStatusText(_("Executing query..."), StatusBar_Status);
  if (!db) return;
  statusbar->SetStatusText(_T("00:00:00"), StatusBar_TimeElapsed);
  statusbar->SetStatusText(wxEmptyString, StatusBar_RowsRetrieved);
}

void ScriptEditorPane::ShowScriptCompleteStatus()
{
  if (!execution->EncounteredErrors())
    statusbar->SetStatusText(_("Query completed successfully"), StatusBar_Status);
  else
    statusbar->SetStatusText(_("Query completed with errors"), StatusBar_Status);
  if (!db) return;
  long time = execution->ElapsedTime();
  wxString elapsed;
  if (time > 5*1000 || (time % 1000) == 0)
    elapsed = wxString::Format(_T("%02ld:%02ld:%02ld"), time / (3600*1000), (time / (60*1000)) % 60, (time / 1000) % 60);
  else
    elapsed = wxString::Format(_T("%02ld:%02ld.%03ld"), (time / (60*1000)) % 60, (time / 1000) % 60, time % 1000);
  statusbar->SetStatusText(elapsed, StatusBar_TimeElapsed);
  statusbar->SetStatusText(wxString::Format(_("%d rows"), execution->TotalRows()), StatusBar_RowsRetrieved);
}

void ScriptEditorPane::OnTimerTick(wxTimerEvent &event)
{
  long time = execution->ElapsedTime();
  wxString elapsed = wxString::Format(_T("%02ld:%02ld:%02ld"), time / (3600*1000), (time / (60*1000)) % 60, (time / 1000) % 60);
  statusbar->SetStatusText(elapsed, StatusBar_TimeElapsed);
}

void ScriptEditorPane::OnExecute(wxCommandEvent &event)
{
  wxASSERT(db != NULL);
  wxASSERT(execution == NULL);

  wxCommandEvent beginEvt = wxCommandEvent(PQWX_ScriptExecutionBeginning);
  beginEvt.SetEventObject(this);
  ProcessEvent(beginEvt);
  ShowScriptInProgressStatus();
  statusUpdateTimer.Start(1000);

  if (resultsBook != NULL) resultsBook->Reset();

  int length;
  wxCharBuffer source = editor->GetRegion(&length);
  execution = new ScriptExecution(this, source, length);
  execution->Proceed();
}

void ScriptEditorPane::OnExecutionFinished(wxCommandEvent &event)
{
  ShowScriptCompleteStatus();
  statusUpdateTimer.Stop();

  delete execution;
  execution = NULL;
}

void ScriptEditorPane::OnQueryComplete(wxCommandEvent &event)
{
  ScriptQueryWork::Result *result = (ScriptQueryWork::Result*) event.GetClientData();
  wxASSERT(result != NULL);
  wxASSERT(execution != NULL);

  execution->ProcessQueryResult(result);
  execution->Proceed();
}

void ScriptEditorPane::OnShowPosition(wxCommandEvent &event)
{
  editor->GotoPos(event.GetInt());
  editor->SetFocus();
}
