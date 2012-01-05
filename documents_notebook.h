// -*- mode: c++ -*-

#ifndef __documents_notebook_h
#define __documents_notebook_h

#include <vector>
#include "wx/notebook.h"

class ScriptEditorPane;
class PQWXDatabaseEvent;
class ServerConnection;

class ConnectableEditor {
public:
  virtual wxString FormatTitle() const = 0;
  virtual bool IsConnected() const = 0;
  virtual const ServerConnection& GetServer() const = 0;
  virtual wxString GetDatabase() const = 0;
};

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
