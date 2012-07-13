#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void SchemaScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row schemaDetail = Query(_T("Schema Detail")).OidParam(nspoid).UniqueResult();
  wxString schemaName = QuoteIdent(schemaDetail[0]);
  switch (mode) {
  case Create: {
    wxString sql;
    sql << _T("CREATE SCHEMA ") << schemaName << _T(" AUTHORIZATION ") << QuoteIdent(schemaDetail[1]);
    *output++ = sql;

    PgAcl(schemaDetail[2]).GenerateGrantStatements(output, schemaDetail[1], schemaName, privilegeMap);
  }
    break;

  case Drop: {
    *output++ = _T("DROP SCHEMA IF EXISTS ") + schemaName;
  }
    break;

  default:
    wxASSERT(false);
  }
}

std::map<wxChar, wxString> SchemaScriptWork::privilegeMap = PrivilegeMap(_T("C=CREATE U=USAGE"));

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
