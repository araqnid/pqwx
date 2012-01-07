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
  EVT_NOTEBOOK_PAGE_CHANGED(Pqwx_DocumentsNotebook, DocumentsNotebook::OnNotebookPageChanged)
END_EVENT_TABLE()

DEFINE_LOCAL_EVENT_TYPE(PQWX_DocumentSelected)
DEFINE_LOCAL_EVENT_TYPE(PQWX_NoDocumentSelected)

ScriptEditorPane* DocumentsNotebook::OpenNewScript()
{
  ScriptEditorPane *pane = new ScriptEditorPane(this, wxID_ANY);
  editors.push_back(pane);
  AddPage(pane, wxEmptyString, true);

  return pane;
}

void DocumentsNotebook::DoClose()
{
  ScriptEditorPane *editor = CurrentEditor();
  if (!editor) return; // e.g. there are no editors and accelerator key activated
  if (editor->IsModified()) {
    wxMessageDialog dbox(this,
			 _("Script is modified, do you want to save it before closing?"),
			 _("Save before closing?"),
			 wxCANCEL | wxYES_NO);
    switch (dbox.ShowModal()) {
    case wxID_YES:
      DoSave();
      break;

    case wxID_CANCEL:
      return;

    case wxID_NO:
      // do nothing...
      break;
    }
  }
  unsigned page = GetSelection();
  DeletePage(page);
  editors.erase(editors.begin() + page);
  EmitDocumentChanged(GetSelection());
}

bool DocumentsNotebook::ConfirmCloseAll()
{
  std::vector<ScriptEditorPane*> modifiedEditors;
  wxString modifiedScriptNames;
  for (std::vector<ScriptEditorPane*>::const_iterator iter = editors.begin(); iter != editors.end(); iter++) {
    if ((*iter)->IsModified()) {
      modifiedEditors.push_back(*iter);
      modifiedScriptNames << (*iter)->GetCoreTitle() << _T('\n');
    }
  }

  if (modifiedEditors.empty()) return true;

  wxString messageText;
  if (modifiedEditors.size() == 1) {
    messageText = wxString::Format(_("%s is modified, do you want to save it before closing?"), modifiedEditors[0]->GetCoreTitle().c_str());
  }
  else {
    messageText = wxString::Format(_("Some scripts have been modified, do you want to save them before closing?\n\n%s"), modifiedScriptNames.c_str());
  }

  wxMessageDialog dbox(this,
		       messageText,
		       _("Save before closing?"),
		       wxCANCEL | wxYES_NO);
  switch (dbox.ShowModal()) {
  case wxID_CANCEL:
    return false;

  case wxID_YES:
    for (std::vector<ScriptEditorPane*>::const_iterator iter = modifiedEditors.begin(); iter != modifiedEditors.end(); iter++) {
      if (!DoSave(*iter)) {
	return false; // save cancelled
      }
    }
    break;

  case wxID_NO:
    // do nothing
    break;
  }

  return true;
}

bool DocumentsNotebook::DoSave(ScriptEditorPane *editor)
{
  if (!editor->IsModified()) return true;
  if (!editor->HasFilename()) {
    return DoSaveAs(editor);
  }
  else {
    editor->SaveFile();
    return true;
  }
}

bool DocumentsNotebook::DoSave()
{
  ScriptEditorPane *editor = CurrentEditor();
  wxASSERT(editor != NULL);
  return DoSave(editor);
}

bool DocumentsNotebook::DoSaveAs(ScriptEditorPane *editor)
{
  wxString filename = wxFileSelector(_("Choose file to save as"),
				     wxEmptyString, // default path
				     editor->GetCoreTitle(),
				     _T(".sql"),
				     _T("*.sql"),
				     wxFD_SAVE | wxFD_OVERWRITE_PROMPT,
				     this
				     );
  if (filename.empty()) return false; // cancelled
  editor->SaveFile(filename);
  return true;
}

bool DocumentsNotebook::DoSaveAs()
{
  ScriptEditorPane *editor = CurrentEditor();
  wxASSERT(editor != NULL);
  return DoSaveAs(editor);
}

inline unsigned DocumentsNotebook::FindPage(ScriptEditorPane *editor) const
{
  unsigned pos = 0;
  for (std::vector<ScriptEditorPane*>::const_iterator iter = editors.begin(); iter != editors.end(); iter++, pos++) {
    if (*iter == editor) return pos;
  }
  wxASSERT(false);
  abort(); // quiet, gcc
}

void DocumentsNotebook::OnScriptStateUpdated(PQWXDatabaseEvent &event)
{
  ScriptEditorPane *editor = dynamic_cast<ScriptEditorPane*>(event.GetEventObject());
  wxASSERT(editor != NULL);
  unsigned page = FindPage(editor);
  SetPageText(page, event.GetString());

  if ((int) page == GetSelection()) {
    // State of current editor has changed
    EmitDocumentChanged(page);
  }
}

void DocumentsNotebook::OnNotebookPageChanged(wxNotebookEvent &event)
{
  EmitDocumentChanged(event.GetSelection());
  event.Skip();
}

void DocumentsNotebook::EmitDocumentChanged(unsigned page)
{
  if (GetPageCount() == 0) {
    wxCommandEvent event(PQWX_NoDocumentSelected);
    ProcessEvent(event);
    return;
  }

  wxASSERT(page < editors.size());
  ScriptEditorPane *editor = editors[page];
  PQWXDatabaseEvent event(editor->GetServer(), editor->GetDatabase(), PQWX_DocumentSelected);
  event.SetEventObject(editor);
  event.SetString(editor->FormatTitle());
  ProcessEvent(event);
}
