#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "pqwx.h"
#include "scripts_notebook.h"

BEGIN_EVENT_TABLE(ScriptsNotebook, wxNotebook)
END_EVENT_TABLE()

void ScriptsNotebook::OpenNewScript() {
  wxMessageBox(_T("TODO: new file"));
}

void ScriptsNotebook::OpenScriptFile(const wxString &filename) {
  wxMessageBox(_T("TODO: open file: ") + filename);
}
