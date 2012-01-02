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
  PQWX_SCRIPT_SELECTED(wxID_ANY, ScriptsNotebook::OnScriptSelected)
END_EVENT_TABLE()

ScriptEditor* ScriptsNotebook::OpenNewScript() {
  scripts.push_back(ScriptModel(GenerateDocumentName()));
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY);
  AddPage(editor, scripts.back().FormatTitle(), true);

  return editor;
}

ScriptEditor* ScriptsNotebook::OpenScriptWithText(const wxString &text) {
  scripts.push_back(ScriptModel(GenerateDocumentName()));
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY);
  AddPage(editor, scripts.back().FormatTitle(), true);
  editor->AddText(text);
  editor->EmptyUndoBuffer();

  return editor;
}

ScriptEditor* ScriptsNotebook::OpenScriptFile(const wxString &filename) {
  wxString tabName;
#ifdef __WXMSW__
  static const wxChar PathSeparator = _T('\\');
#else
  static const wxChar PathSeparator = _T('/');
#endif

  size_t slash = filename.find_last_of(PathSeparator);
  if (slash == wxString::npos)
    tabName = filename;
  else
    tabName = filename.Mid(slash + 1);

  documentCounter++;

  scripts.push_back(ScriptModel(tabName, filename));
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY);
  AddPage(editor, scripts.back().FormatTitle(), true);

  return editor;

}

ScriptModel& ScriptsNotebook::FindScriptForEditor(const ScriptEditor *editor)
{
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

unsigned ScriptsNotebook::FindScriptPage(const ScriptModel &script) const
{
  unsigned pos = 0;
  for (std::vector<ScriptModel>::const_iterator iter = scripts.begin(); iter != scripts.end(); iter++, pos++) {
    if (&(*iter) == &script) return pos;
  }
  wxASSERT(false);
  abort(); // quiet, gcc
}

void ScriptsNotebook::OnScriptSelected(PQWXDatabaseEvent& event)
{
  event.Skip(); // want the frame to process this as well
  ScriptEditor *editor = (ScriptEditor*) event.GetEventObject();
  unsigned page = FindScriptPage(FindScriptForEditor(editor));
  SetPageText(page, event.GetString());
}
