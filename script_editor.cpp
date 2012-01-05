#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <fstream>
#include "wx/stc/stc.h"
#include "script_editor.h"
#include "script_events.h"
#include "database_work.h"
#include "script_query_work.h"
#include "postgresql_wordlists_yml.h"

BEGIN_EVENT_TABLE(ScriptEditor, wxStyledTextCtrl)
  EVT_SET_FOCUS(ScriptEditor::OnSetFocus)
  EVT_KILL_FOCUS(ScriptEditor::OnLoseFocus)
  EVT_STC_SAVEPOINTLEFT(wxID_ANY, ScriptEditor::OnSavePointLeft)
  EVT_STC_SAVEPOINTREACHED(wxID_ANY, ScriptEditor::OnSavePointReached)
  PQWX_SCRIPT_EXECUTE(wxID_ANY, ScriptEditor::OnExecute)
  PQWX_SCRIPT_DISCONNECT(wxID_ANY, ScriptEditor::OnDisconnect)
  PQWX_SCRIPT_RECONNECT(wxID_ANY, ScriptEditor::OnReconnect)
  PQWX_SCRIPT_QUERY_COMPLETE(wxID_ANY, ScriptEditor::OnQueryComplete)
END_EVENT_TABLE()

DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptStateUpdated)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptExecutionBeginning)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptExecutionFinishing)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptQueryComplete)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptConnectionStatus)

int ScriptEditor::documentCounter = 0;

ScriptEditor::ScriptEditor(wxWindow *parent, wxWindowID id)
: wxStyledTextCtrl(parent, id), db(NULL), lexer(NULL), resultsHandler(NULL)
{
  SetLexer(wxSTC_LEX_SQL);
#if wxUSE_UNICODE
  SetCodePage(wxSTC_CP_UTF8);
#endif
  SetKeyWords(0, postgresql_wordlists_yml_keywords);
  SetKeyWords(1, postgresql_wordlists_yml_database_objects);
  SetKeyWords(3, postgresql_wordlists_yml_sqlplus);
  SetKeyWords(4, postgresql_wordlists_yml_user1);
  SetKeyWords(5, postgresql_wordlists_yml_user2);
  SetKeyWords(6, postgresql_wordlists_yml_user3);
  SetKeyWords(7, postgresql_wordlists_yml_user4);

  StyleClearAll();

  // taken from sql.properties in scite
  StyleSetSpec(wxSTC_SQL_DEFAULT, _T("fore:#808080"));
  StyleSetSpec(wxSTC_SQL_COMMENT, _T("fore:#007f00")); // + font.comment
  StyleSetSpec(wxSTC_SQL_COMMENTLINE, _T("fore:#007f00")); // + font.comment
  StyleSetSpec(wxSTC_SQL_COMMENTDOC, _T("fore:#7f7f7f"));
  StyleSetSpec(wxSTC_SQL_NUMBER, _T("fore:#007f7f"));
  StyleSetSpec(wxSTC_SQL_WORD, _T("fore:#00007F,bold"));
  StyleSetSpec(wxSTC_SQL_STRING, _T("fore:#7f007f")); // + font.monospace
  StyleSetSpec(wxSTC_SQL_CHARACTER, _T("fore:#7f007f")); // + font.monospace
  StyleSetSpec(wxSTC_SQL_SQLPLUS, _T("fore:#7F7F00")); // colour.preproc
  StyleSetSpec(wxSTC_SQL_SQLPLUS_PROMPT, _T("fore:#007F00,back:#E0FFE0,eolfilled")); // + font.monospace
  StyleSetSpec(wxSTC_SQL_OPERATOR, _T("bold"));
  StyleSetSpec(wxSTC_SQL_IDENTIFIER, _T(""));
  StyleSetSpec(wxSTC_SQL_COMMENTLINEDOC, _T("fore:#007f00")); // + font.comment
  StyleSetSpec(wxSTC_SQL_WORD2, _T("fore:#b00040"));
  StyleSetSpec(wxSTC_SQL_COMMENTDOCKEYWORD, _T("fore:#3060a0")); // + font.code.comment.doc
  StyleSetSpec(wxSTC_SQL_COMMENTDOCKEYWORDERROR, _T("fore:#804020")); // + font.code.comment.doc
  StyleSetSpec(wxSTC_SQL_USER1, _T("fore:#4b0082"));
  StyleSetSpec(wxSTC_SQL_USER2, _T("fore:#b00040"));
  StyleSetSpec(wxSTC_SQL_USER3, _T("fore:#8b0000"));
  StyleSetSpec(wxSTC_SQL_USER4, _T("fore:#800080"));

  coreTitle = wxString::Format(_("Query-%d.sql"), ++documentCounter);
}

void ScriptEditor::OnSetFocus(wxFocusEvent &event)
{
  event.Skip();
  EmitScriptSelected();
}

void ScriptEditor::OnLoseFocus(wxFocusEvent &event)
{
  event.Skip();
}

void ScriptEditor::OnSavePointLeft(wxStyledTextEvent &event)
{
  modified = true;
  EmitScriptSelected();
}

void ScriptEditor::OnSavePointReached(wxStyledTextEvent &event)
{
  modified = false;
  EmitScriptSelected();
}

void ScriptEditor::Connect(const ServerConnection &server_, const wxString &dbname)
{
  wxASSERT(db == NULL);
  server = server_;
  db = new DatabaseConnection(server, dbname.empty() ? server.globalDbName : dbname, _("Query"));
  // TODO handle connection problems, direct through connection dialogue
  db->Connect();
  state = Idle;
  db->AddWork(new SetupNoticeProcessorWork(this));
  EmitScriptSelected();
}

void ScriptEditor::SetConnection(const ServerConnection &server_, DatabaseConnection *db_)
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
  EmitScriptSelected();
}

void ScriptEditor::EmitScriptSelected()
{
  wxString dbname;
  if (db) dbname = db->DbName();
  PQWXDatabaseEvent selectionChangedEvent(server, dbname, PQWX_ScriptStateUpdated);
  selectionChangedEvent.SetString(FormatTitle());
  selectionChangedEvent.SetEventObject(this);
  if (db) selectionChangedEvent.SetConnectionState(state);
  ProcessEvent(selectionChangedEvent);
}

void ScriptEditor::OnExecute(wxCommandEvent &event)
{
  wxASSERT(db != NULL);
  wxASSERT(lexer == NULL);

  wxCommandEvent beginEvt = wxCommandEvent(PQWX_ScriptExecutionBeginning);
  beginEvt.SetEventObject(this);
  ProcessEvent(beginEvt);
  // the handler should give us an object to pass results to
  wxASSERT(beginEvt.GetEventObject() != this);
  resultsHandler = dynamic_cast<ExecutionResultsHandler*>(beginEvt.GetEventObject());
  wxASSERT(resultsHandler != NULL);

  int start, end;
  GetSelection(&start, &end);

  int length;

  if (start == end) {
    source = GetTextRaw();
    length = GetLength();
  }
  else {
    source = GetTextRangeRaw(start, end);
    length = end - start;
  }

  // wxCharBuffer works like auto_ptr, so can't copy it.
  lexer = new ExecutionLexer(source.data(), length);

  while (true) {
    if (!ProcessExecution()) {
      break;
    }
  }
}

bool ScriptEditor::ProcessExecution()
{
  ExecutionLexer::Token t = lexer->Pull();
  if (t.type == ExecutionLexer::Token::END) {
    FinishExecution();
    return false;
  }

  if (t.type == ExecutionLexer::Token::SQL) {
    bool added = db->AddWorkOnlyIfConnected(new ScriptQueryWork(this, t));
    wxASSERT(added);
    return false;
  }
  else {
    wxLogDebug(_T("psql | %s"), lexer->GetWXString(t).c_str());
    return true;
  }
}

void ScriptEditor::FinishExecution()
{
  wxCommandEvent finishEvt = wxCommandEvent(PQWX_ScriptExecutionFinishing);
  finishEvt.SetEventObject(this);
  ProcessEvent(finishEvt);

  delete lexer;

  lexer = NULL;
  resultsHandler = NULL;
}

void ScriptEditor::OnQueryComplete(wxCommandEvent &event)
{
  ScriptQueryWork::Result *result = (ScriptQueryWork::Result*) event.GetClientData();
  wxASSERT(result != NULL);

  if (result->status == PGRES_TUPLES_OK) {
    wxLogDebug(_T("%s (%lu tuples)"), result->statusTag.c_str(), result->data->size());
    resultsHandler->ScriptResultSet(result->statusTag, *result->data);
  }
  else if (result->status == PGRES_COMMAND_OK) {
    wxLogDebug(_T("%s (no tuples)"), result->statusTag.c_str());
    resultsHandler->ScriptCommandCompleted(result->statusTag);
  }
  else if (result->status == PGRES_FATAL_ERROR) {
    wxLogDebug(_T("Got error: %s"), result->error.primary.c_str());
    resultsHandler->ScriptError(result->error);
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

void ScriptEditorNoticeReceiver(void *arg, const PGresult *rs)
{
  ScriptEditor *editor = (ScriptEditor*) arg;
  editor->OnConnectionNotice(rs);
}

void ScriptEditor::OnConnectionNotice(const PGresult *rs)
{
  wxASSERT(PQresultStatus(rs) == PGRES_NONFATAL_ERROR);
  PgError error(rs);
  if (resultsHandler != NULL)
    resultsHandler->ScriptNotice(error);
  else
    wxLogDebug(_T("asynchronous diagnostic: (%s) %s"), error.severity.c_str(), error.primary.c_str());
}

void ScriptEditor::OnDisconnect(wxCommandEvent &event)
{
  wxASSERT(lexer == NULL); // must not be executing
  wxASSERT(db != NULL);
  db->Dispose();
  delete db;
  db = NULL;
  EmitScriptSelected(); // refresh title etc
}

void ScriptEditor::OnReconnect(wxCommandEvent &event)
{
  wxASSERT(lexer == NULL); // must not be executing

  ConnectDialogue *dbox = new ConnectDialogue(NULL, new ChangeScriptConnection(this));
  if (db != NULL)
    dbox->Suggest(server);
  dbox->Show();
  dbox->SetFocus();
}

void ScriptEditor::OpenFile(const wxString &filename) {
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

  LoadFile(filename);
  SetSavePoint();
  EmptyUndoBuffer();
  EmitScriptSelected();
}

void ScriptEditor::Populate(const wxString &text) {
  AddText(text);
  EmptyUndoBuffer();
}

void ScriptEditor::LoadFile(const wxString &filename)
{
  // assume input files are in the correct coding system for now
  std::ifstream input(filename.utf8_str(), std::ifstream::in);
  char buf[8193];
  while (input.good()) {
    input.read(buf, sizeof(buf));
    size_t got = input.gcount();
    buf[got] = '\0';
    AddTextRaw(buf);
  }
}

wxString ScriptEditor::FormatTitle() {
  wxString output;
  if (db == NULL)
    output << _("<disconnected>");
  else
    output << db->Identification();
  output << _T(" - ") << coreTitle;
  if (modified) output << _T(" *");
  return output;
}
