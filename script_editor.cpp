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

BEGIN_EVENT_TABLE(ScriptEditor, wxStyledTextCtrl)
  EVT_SET_FOCUS(ScriptEditor::OnSetFocus)
END_EVENT_TABLE()

ScriptEditor::ScriptEditor(ScriptsNotebook *owner, wxWindowID id) : wxStyledTextCtrl(owner, id), owner(owner)
{
  SetLexer(wxSTC_LEX_SQL);
  SetCodePage(wxSTC_CP_UTF8);
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

void ScriptEditor::OnSetFocus(wxFocusEvent &event) {
  wxStyledTextCtrl::OnGainFocus(event);

  ScriptModel& script = owner->FindScriptForEditor(this);

  wxCommandEvent selectionChangedEvent(PQWX_SCRIPT_SELECTED);
  selectionChangedEvent.SetString(script.title);
  ProcessEvent(selectionChangedEvent);
}
