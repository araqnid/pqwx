#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser.h"
#include "object_browser_model.h"
#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void TablespaceScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row detail = Query(_T("Tablespace Detail")).OidParam(spcoid).UniqueResult();
  wxString name = QuoteIdent(detail[0]);
  wxString owner = QuoteIdent(detail[1]);
  wxString location = QuoteLiteral(detail[3]);
  switch (mode) {
  case Create: {
    wxString sql;
    sql << _T("CREATE TABLESPACE ") << name;
    sql << _T(" OWNER ") << owner;
    sql << _T(" LOCATION ") << location;

    *output++ = sql;

    PgAcl(detail[2]).GenerateGrantStatements(output, owner, _T("TABLESPACE ") + name, privilegeMap);
    PgSettings(detail[4]).GenerateSetStatements(output, this, _T("ALTER TABLESPACE ") + name + _T(' '), true);
  }
    break;

  case Alter: {
    *output++ = _T("ALTER TABLESPACE ") + name + _T(" RENAME TO ") + name;
    *output++ = _T("ALTER TABLESPACE ") + name + _T(" OWNER TO ") + owner;

    PgAcl(detail[2]).GenerateGrantStatements(output, owner, _T("TABLESPACE ") + name, privilegeMap);
    PgSettings(detail[4]).GenerateSetStatements(output, this, _T("ALTER TABLESPACE ") + name + _T(' '), true);
  }
    break;

  case Drop: {
    *output++ = _T("DROP TABLESPACE ") + name;
  }
    break;

  default:
    wxASSERT(false);
  }
}

std::map<wxChar, wxString> TablespaceScriptWork::privilegeMap = PrivilegeMap(_T("C=CREATE"));

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
