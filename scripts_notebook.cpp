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

BEGIN_EVENT_TABLE(ScriptsNotebook, wxNotebook)
END_EVENT_TABLE()

void ScriptsNotebook::OpenNewScript() {
  wxString tabName;
  tabName << _("Query-") << ++documentCounter;

  ScriptModel *model = new ScriptModel(tabName);
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY, model);
  AddPage(editor, tabName, true);
}

void ScriptsNotebook::OpenScriptFile(const wxString &filename) {
  wxString tabName;

  size_t slash = filename.find_last_of(_T('/'));
  if (slash == wxString::npos)
    tabName = filename;
  else
    tabName = filename.Mid(slash + 1);

  ScriptModel *model = new ScriptModel(tabName, filename);
  ScriptEditor *editor = new ScriptEditor(this, wxID_ANY, model);
  AddPage(editor, tabName, true);
}
