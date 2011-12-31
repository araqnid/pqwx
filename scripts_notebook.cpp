#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/stc/stc.h"
#include "pqwx.h"
#include "scripts_notebook.h"
#include "script_editor.h"
#include "script_events.h"

BEGIN_EVENT_TABLE(ScriptsNotebook, wxNotebook)
END_EVENT_TABLE()

DEFINE_LOCAL_EVENT_TYPE(PQWX_SCRIPT_SELECTED)

void ScriptsNotebook::OpenNewScript() {
  wxString tabName;
  tabName << _("Query-") << ++documentCounter;

  scripts.push_back(ScriptModel(tabName));
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY);
  AddPage(editor, tabName, true);
  editor->SetFocus();
}

void ScriptsNotebook::OpenScriptWithText(const wxString &text) {
  wxString tabName;
  tabName << _("Query-") << ++documentCounter;

  scripts.push_back(ScriptModel(tabName));
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY);
  editor->AddText(text);
  AddPage(editor, tabName, true);
  editor->SetFocus();
}

void ScriptsNotebook::OpenScriptFile(const wxString &filename) {
  wxString tabName;

  size_t slash = filename.find_last_of(_T('/'));
  if (slash == wxString::npos)
    tabName = filename;
  else
    tabName = filename.Mid(slash + 1);

  scripts.push_back(ScriptModel(tabName, filename));
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY);
  AddPage(editor, tabName, true);
  editor->SetFocus();
}

ScriptModel& ScriptsNotebook::FindScriptForEditor(const ScriptEditor *editor) {
  for (size_t i = 0; i < GetPageCount(); i++) {
    wxNotebookPage *page = GetPage(i);
    if (page == editor) {
      wxASSERT(i < scripts.size());
      return scripts[i];
    }
  }
  wxASSERT(false);
  abort(); // quiet, gcc
}
