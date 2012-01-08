/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __documents_notebook_h
#define __documents_notebook_h

#include <vector>
#include "wx/notebook.h"

class ScriptEditorPane;
class PQWXDatabaseEvent;
class ServerConnection;

/**
 * Interface implemented by editors within the notebook.
 */
class ConnectableEditor {
public:
  virtual wxString FormatTitle() const = 0;
  virtual bool IsConnected() const = 0;
  virtual const ServerConnection& GetServer() const = 0;
  virtual wxString GetDatabase() const = 0;
  virtual bool IsExecuting() const = 0;
  virtual bool IsModified() const = 0;
};

/**
 * A notebook containing an editor in each tab.
 */
class DocumentsNotebook : public wxNotebook {
public:
  /**
   * Create notebook
   */
  DocumentsNotebook(wxWindow *parent, wxWindowID id) : wxNotebook(parent, id) { }

  ~DocumentsNotebook() { Dispose(); }

  /**
   * Close all scripts and dispose of their resources.
   */
  void Dispose();

  /**
   * Open a new editor.
   */
  ScriptEditorPane* OpenNewScript();
  /**
   * Close the current editor.
   */
  void DoClose();
  /**
   * Save the content of the current editor, if it is modified.
   */
  bool DoSave();
  /**
   * Save the content of the current editor to some new filename.
   */
  bool DoSaveAs();

  /**
   * Check for unmodified scripts and confirm closing with the user.
   *
   * @return true if closing is ok
   */
  bool ConfirmCloseAll();

private:
  std::vector<ScriptEditorPane*> editors;
  unsigned FindPage(ScriptEditorPane*) const;
  void EmitDocumentChanged(unsigned page);
  ScriptEditorPane *CurrentEditor() const {
    if (editors.empty()) return NULL;
    unsigned n = GetSelection();
    wxASSERT(n <= editors.size());
    return editors[n];
  }

  void OnScriptStateUpdated(PQWXDatabaseEvent&);
  void OnNotebookPageChanged(wxNotebookEvent&);

  bool DoSave(ScriptEditorPane *editor);
  bool DoSaveAs(ScriptEditorPane *editor);

  DECLARE_EVENT_TABLE();
};

#endif

// Local Variables:
// mode: c++
// End:
