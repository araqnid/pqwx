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
#include "postgresql_wordlists_yml.h"
#include "script_editor_pane.h"

BEGIN_EVENT_TABLE(ScriptEditor, wxStyledTextCtrl)
  EVT_SET_FOCUS(ScriptEditor::OnSetFocus)
  EVT_KILL_FOCUS(ScriptEditor::OnLoseFocus)
  EVT_STC_SAVEPOINTLEFT(wxID_ANY, ScriptEditor::OnSavePointLeft)
  EVT_STC_SAVEPOINTREACHED(wxID_ANY, ScriptEditor::OnSavePointReached)
END_EVENT_TABLE()

DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptStateUpdated)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptExecutionBeginning)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptExecutionFinishing)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptQueryComplete)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptConnectionStatus)

ScriptEditor::ScriptEditor(wxWindow *parent, wxWindowID id, ScriptEditorPane *owner)
  : wxStyledTextCtrl(parent, id), owner(owner)
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
}

inline void ScriptEditor::EmitScriptSelected()
{
  owner->EmitScriptSelected();
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
  owner->MarkModified(true);
}

void ScriptEditor::OnSavePointReached(wxStyledTextEvent &event)
{
  owner->MarkModified(false);
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

  SetSavePoint();
  EmptyUndoBuffer();
}

wxCharBuffer ScriptEditor::GetRegion(int *lengthp)
{
  int start, end;
  GetSelection(&start, &end);

  if (start == end) {
    *lengthp = GetLength();
    return GetTextRaw();
  }
  else {
    *lengthp = end - start;
    return GetTextRangeRaw(start, end);
  }
}
