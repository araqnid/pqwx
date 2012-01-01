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

ScriptEditor* ScriptsNotebook::OpenNewScript() {
  scripts.push_back(ScriptModel(GenerateDocumentName()));
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY);
  AddPage(editor, scripts.back().FormatTitle(), true);
  editor->SetFocus();

  return editor;
}

ScriptEditor* ScriptsNotebook::OpenScriptWithText(const wxString &text) {
  scripts.push_back(ScriptModel(GenerateDocumentName()));
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY);
  AddPage(editor, scripts.back().FormatTitle(), true);
  editor->AddText(text);
  editor->EmptyUndoBuffer();
  editor->SetFocus();

  return editor;
}

ScriptEditor* ScriptsNotebook::OpenScriptFile(const wxString &filename) {
  wxString tabName;

  size_t slash = filename.find_last_of(_T('/'));
  if (slash == wxString::npos)
    tabName = filename;
  else
    tabName = filename.Mid(slash + 1);

  documentCounter++;

  scripts.push_back(ScriptModel(tabName, filename));
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY);
  AddPage(editor, scripts.back().FormatTitle(), true);
  editor->SetFocus();

  return editor;

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

unsigned ScriptsNotebook::FindScriptPage(const ScriptModel &script) const {
  unsigned pos = 0;
  for (std::vector<ScriptModel>::const_iterator iter = scripts.begin(); iter != scripts.end(); iter++, pos++) {
    if (&(*iter) == &script) return pos;
  }
  wxASSERT(false);
  abort(); // quiet, gcc
}

void ScriptsNotebook::MarkScriptModified(ScriptModel &script)
{
  script.modified = true;
  EmitScriptSelected(script);
  SetPageText(FindScriptPage(script), script.FormatTitle());
}

void ScriptsNotebook::MarkScriptUnmodified(ScriptModel &script)
{
  script.modified = false;
  EmitScriptSelected(script);
  SetPageText(FindScriptPage(script), script.FormatTitle());
}

void ScriptsNotebook::UpdateScriptDatabase(ScriptModel &script, const wxString &database) {
  script.database = database;
  EmitScriptSelected(script);
  SetPageText(FindScriptPage(script), script.FormatTitle());
}

void ScriptsNotebook::EmitScriptSelected(ScriptModel &script)
{
  wxCommandEvent selectionChangedEvent(PQWX_SCRIPT_SELECTED);
  selectionChangedEvent.SetString(script.FormatTitle());
  ProcessEvent(selectionChangedEvent);
}
