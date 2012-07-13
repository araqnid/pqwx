#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void ViewScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row viewDetail = Query(_T("View Detail")).OidParam(reloid).UniqueResult();
  QueryResults columns = Query(_T("Relation Column Detail")).OidParam(reloid).List();
  wxString viewName = QuoteIdent(viewDetail[1]) + _T('.') + QuoteIdent(viewDetail[0]);

  switch (mode) {
  case Create:
  case Alter: {
    wxString definition = viewDetail.ReadText(3);

    wxString sql;
    if (mode == Create)
      sql << _T("CREATE VIEW ");
    else
      sql << _T("CREATE OR REPLACE VIEW ");
    sql << viewName << _T(" AS\n") << definition;
    *output++ = sql;

    PgAcl(viewDetail[4]).GenerateGrantStatements(output, viewDetail[2], viewName, privilegeMap);

    AddDescription(output, _T("VIEW"), viewName, viewDetail[5]);
  }
    break;

  case Select: {
    wxString sql;
    sql << _T("SELECT ");
    unsigned n = 0;
    for (QueryResults::const_iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name((*iter).ReadText(0));
      sql << QuoteIdent(name);
      if (n != (columns.size()-1))
        sql << _T(",\n       ");
      else
        sql << _T("\n");
    }
    sql << _T("FROM ") << viewName;
    *output++ = sql;
  }
    break;

  case Drop: {
    wxString sql;
    sql << _T("DROP VIEW IF EXISTS ") << viewName;

    *output++ = sql;
  }
    break;

  default:
    wxASSERT(false);
  }
}

std::map<wxChar, wxString> ViewScriptWork::privilegeMap = PrivilegeMap(_T("r=SELECT"));

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
