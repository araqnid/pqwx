// -*- mode: c++ -*-

#ifndef __documents_notebook_h
#define __documents_notebook_h

#include <vector>
#include "wx/notebook.h"

class ScriptEditorPane;
class PQWXDatabaseEvent;

class DocumentsNotebook : public wxNotebook {
public:
  DocumentsNotebook(wxWindow *parent, wxWindowID id) : wxNotebook(parent, id) { }
  ScriptEditorPane* OpenNewScript();

  void OnScriptStateUpdated(PQWXDatabaseEvent&);
  void OnPageChanged(wxNotebookEvent&);

private:
  std::vector<ScriptEditorPane*> editors;
  unsigned FindPage(ScriptEditorPane*) const;
  void EmitDocumentChanged(unsigned page);

  DECLARE_EVENT_TABLE();
};

#endif
