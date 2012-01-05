#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "pqwx.h"
#include "documents_notebook.h"
#include "script_editor.h"
#include "script_editor_pane.h"
#include "script_events.h"

BEGIN_EVENT_TABLE(DocumentsNotebook, wxNotebook)
  PQWX_SCRIPT_STATE_UPDATED(wxID_ANY, DocumentsNotebook::OnScriptStateUpdated)
  EVT_NOTEBOOK_PAGE_CHANGED(wxID_ANY, DocumentsNotebook::OnPageChanged)
END_EVENT_TABLE()

DEFINE_LOCAL_EVENT_TYPE(PQWX_DocumentSelected)

ScriptEditor* DocumentsNotebook::OpenNewScript()
{
  ScriptEditorPane *pane = new ScriptEditorPane(this, wxID_ANY);
  editors.push_back(pane->GetEditor());
  AddPage(pane, wxEmptyString, true);

  return pane->GetEditor();
}

inline unsigned DocumentsNotebook::FindPage(ScriptEditor *editor) const
{
  unsigned pos = 0;
  for (std::vector<ScriptEditor*>::const_iterator iter = editors.begin(); iter != editors.end(); iter++, pos++) {
    if (*iter == editor) return pos;
  }
  wxASSERT(false);
  abort(); // quiet, gcc
}

void DocumentsNotebook::OnScriptStateUpdated(PQWXDatabaseEvent &event)
{
  ScriptEditor *editor = dynamic_cast<ScriptEditor*>(event.GetEventObject());
  wxASSERT(editor != NULL);
  unsigned page = FindPage(editor);
  SetPageText(page, event.GetString());

  if ((int) page == GetSelection()) {
    // State of current editor has changed
    EmitDocumentChanged(page);
  }
}

void DocumentsNotebook::OnPageChanged(wxNotebookEvent &event)
{
  EmitDocumentChanged(event.GetSelection());
}

void DocumentsNotebook::EmitDocumentChanged(unsigned page)
{
  wxASSERT(page < editors.size());
  ScriptEditor *editor = editors[page];
  wxCommandEvent event(PQWX_DocumentSelected);
  event.SetEventObject(editor);
  event.SetString(editor->FormatTitle());
  ProcessEvent(event);
}
