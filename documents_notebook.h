// -*- mode: c++ -*-

#ifndef __documents_notebook_h
#define __documents_notebook_h

#include <vector>
#include "wx/notebook.h"

class ScriptEditor;
class PQWXDatabaseEvent;

class DocumentsNotebook : public wxNotebook {
public:
  DocumentsNotebook(wxWindow *parent, wxWindowID id) : wxNotebook(parent, id) { }
  ScriptEditor* OpenNewScript();

  void OnScriptStateUpdated(PQWXDatabaseEvent&);
  void OnPageChanged(wxNotebookEvent&);

private:
  std::vector<ScriptEditor*> editors;
  unsigned FindPage(ScriptEditor*) const;
  void EmitDocumentChanged(unsigned page);

  DECLARE_EVENT_TABLE();
};

#endif
