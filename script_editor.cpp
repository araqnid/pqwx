#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/stc/stc.h"
#include "scripts_notebook.h"
#include "script_editor.h"
#include "script_events.h"
#include "database_work.h"
#include "script_query_work.h"

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

DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptSelected)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptExecutionBeginning)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptExecutionFinishing)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptQueryComplete)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptConnectionStatus)

ScriptEditor::ScriptEditor(ScriptsNotebook *owner, wxWindowID id)
: wxStyledTextCtrl(owner, id), owner(owner), db(NULL), lexer(NULL), resultsHandler(NULL)
{
  SetLexer(wxSTC_LEX_SQL);
#if wxUSE_UNICODE
  SetCodePage(wxSTC_CP_UTF8);
#endif
  // taken from sql.properties in scite
  SetKeyWords(0, _T("absolute action add admin after aggregate \
alias all allocate alter and any are array as asc \
assertion at authorization \
before begin binary bit blob body boolean both breadth by \
call cascade cascaded case cast catalog char character \
check class clob close collate collation column commit \
completion connect connection constraint constraints \
constructor continue corresponding create cross cube current \
current_date current_path current_role current_time current_timestamp \
current_user cursor cycle \
data date day deallocate dec decimal declare default \
deferrable deferred delete depth deref desc describe descriptor \
destroy destructor deterministic dictionary diagnostics disconnect \
distinct domain double drop dynamic \
each else end end-exec equals escape every except \
exception exec execute exists exit external \
false fetch first float for foreign found from free full \
function \
general get global go goto grant group grouping \
having host hour \
identity if ignore immediate in indicator initialize initially \
inner inout input insert int integer intersect interval \
into is isolation iterate \
join \
key \
language large last lateral leading left less level like \
limit local localtime localtimestamp locator \
map match minute modifies modify module month \
names national natural nchar nclob new next no none \
not null numeric \
object of off old on only open operation option \
or order ordinality out outer output \
package pad parameter parameters partial path postfix precision prefix \
preorder prepare preserve primary \
prior privileges procedure public \
read reads real recursive ref references referencing relative \
restrict result return returns revoke right \
role rollback rollup routine row rows \
savepoint schema scroll scope search second section select \
sequence session session_user set sets size smallint some| space \
specific specifictype sql sqlexception sqlstate sqlwarning start \
state statement static structure system_user \
table temporary terminate than then time timestamp \
timezone_hour timezone_minute to trailing transaction translation \
treat trigger true \
under union unique unknown \
unnest update usage user using \
value values varchar variable varying view \
when whenever where with without work write \
year \
zone"));
  SetKeyWords(1, _T("all alter and any array as asc at authid avg begin between \
binary_integer \
body boolean bulk by char char_base check close cluster collect \
comment commit compress connect constant create current currval \
cursor date day declare decimal default delete desc distinct \
do drop else elsif end exception exclusive execute exists exit \
extends false fetch float for forall from function goto group \
having heap hour if immediate in index indicator insert integer \
interface intersect interval into is isolation java level like \
limited lock long loop max min minus minute mlslabel mod mode \
month natural naturaln new nextval nocopy not nowait null number \
number_base ocirowid of on opaque open operator option or order \
organization others out package partition pctfree pls_integer \
positive positiven pragma prior private procedure public raise \
range raw real record ref release return reverse rollback row \
rowid rownum rowtype savepoint second select separate set share \
smallint space sql sqlcode sqlerrm start stddev subtype successful \
sum synonym sysdate table then time timestamp to trigger true \
type uid union unique update use user validate values varchar \
varchar2 variance view when whenever where while with work write \
year zone"));

  StyleClearAll();

  StyleSetSpec(wxSTC_SQL_DEFAULT, _T("fore:#808080"));
  StyleSetSpec(wxSTC_SQL_COMMENT, _T("fore:#007f00"));
  StyleSetSpec(wxSTC_SQL_COMMENTLINE, _T("fore:#007f00"));
  StyleSetSpec(wxSTC_SQL_COMMENTDOC, _T("fore:#7f7f7f"));
  StyleSetSpec(wxSTC_SQL_NUMBER, _T("fore:#007f7f"));
  StyleSetSpec(wxSTC_SQL_WORD, _T("fore:#00007F,bold"));
  StyleSetSpec(wxSTC_SQL_STRING, _T("fore:#7f007f"));
  StyleSetSpec(wxSTC_SQL_CHARACTER, _T("fore:#7f007f"));
  StyleSetSpec(wxSTC_SQL_OPERATOR, _T("bold"));
  StyleSetSpec(wxSTC_SQL_IDENTIFIER, _T(""));
  StyleSetSpec(wxSTC_SQL_COMMENTLINEDOC, _T("fore:#007f00"));
  StyleSetSpec(wxSTC_SQL_WORD2, _T("fore:#b00040"));
  StyleSetSpec(wxSTC_SQL_COMMENTDOCKEYWORD, _T("fore:#3060a0"));
  StyleSetSpec(wxSTC_SQL_COMMENTDOCKEYWORDERROR, _T("fore:#804020"));
  StyleSetSpec(wxSTC_SQL_USER1, _T("fore:#4b0082"));
  StyleSetSpec(wxSTC_SQL_USER2, _T("fore:#b00040"));
  StyleSetSpec(wxSTC_SQL_USER3, _T("fore:#8b0000"));
  StyleSetSpec(wxSTC_SQL_USER4, _T("fore:#800080"));
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
  ScriptModel& script = owner->FindScriptForEditor(this);
  script.modified = true;
  EmitScriptSelected();
}

void ScriptEditor::OnSavePointReached(wxStyledTextEvent &event)
{
  ScriptModel& script = owner->FindScriptForEditor(this);
  script.modified = false;
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
  ScriptModel& script = owner->FindScriptForEditor(this);
  script.database = db->Identification();
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
  ScriptModel& script = owner->FindScriptForEditor(this);
  script.database = db->Identification();
  state = Idle;
  EmitScriptSelected();
}

void ScriptEditor::EmitScriptSelected()
{
  wxString dbname;
  if (db) dbname = db->DbName();
  PQWXDatabaseEvent selectionChangedEvent(server, dbname, PQWX_ScriptSelected);
  selectionChangedEvent.SetString(owner->FindScriptForEditor(this).FormatTitle());
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
    wxLogDebug(_T("%s (%lu tuples)"), result->statusTag.c_str(), result->data.size());
    resultsHandler->ScriptResultSet(result->statusTag, result->fields, result->data);
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
  ScriptModel& script = owner->FindScriptForEditor(this);
  script.database = wxEmptyString;
  EmitScriptSelected(); // recalculate title etc
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
