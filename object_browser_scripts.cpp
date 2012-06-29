#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "pqwx.h"

#include "object_browser.h"
#include "object_browser_model.h"
#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"
#include "script_events.h"
#include "sql_dictionary_file.h"

#include "wx/clipbrd.h"

const SqlDictionary& ScriptWork::GetSqlDictionary()
{
  static SqlDictionaryFile sqlDictionary(_T("memory:ObjectBrowser/object_browser_scripts.sql"));
  return sqlDictionary;
}

void ScriptWork::OnCompletion()
{
  ObjectBrowserModel& obm = ::wxGetApp().GetObjectBrowserModel();

  switch (output) {
  case Window: {
    const DatabaseModel *database;
    switch (targetRef.GetObjectClass()) {
    case ObjectModelReference::PG_DATABASE:
      database = obm.FindDatabase(targetRef);
      break;
    case InvalidOid:
      database = obm.FindAdminDatabase(targetRef);
      break;
    default:
      wxASSERT(false);
      return;
    }
    PQWXDatabaseEvent evt(database->server->conninfo, database->name, PQWX_ScriptToWindow);
    evt.SetString(script);
    view->ProcessEvent(evt);
    return;
  }
    break;

  case File:
    wxMessageBox(_T("TODO"));
    break;

  case Clipboard:
    if (wxTheClipboard->Open()) {
      wxTheClipboard->SetData(new wxTextDataObject(script));
      wxTheClipboard->Close();
    }
    break;
  default:
    wxASSERT(false);
  }
}

std::map<wxChar, wxString> ScriptWork::PrivilegeMap(const wxString &spec)
{
  std::map<wxChar, wxString> privilegeMap;
  wxStringTokenizer tkz(spec, _T(", \n\t"));
  while (tkz.HasMoreTokens()) {
    wxString token = tkz.GetNextToken();
    wxASSERT(token.length() > 2);
    privilegeMap[token[0]] = token.Mid(2);
  }
  return privilegeMap;
}

wxRegEx PgAcl::AclEntry::pattern(_T("^(.*)=([a-zA-Z*]*)/(.+)"));

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
