#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void RoleScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row detail = Query(_T("Role Detail")).OidParam(roloid).UniqueResult();
  wxString name = QuoteIdent(detail[0]);
  switch (mode) {
  case Create:
  case Alter: {
    wxString sql;
    sql << (mode == Create ? _T("CREATE") : _T("ALTER")) << _T(" ROLE ") << name;
    if (detail.ReadBool(1)) sql << _T("\n\tSUPERUSER");
    if (detail.ReadBool(2)) sql << _T("\n\tINHERIT");
    if (detail.ReadBool(3)) sql << _T("\n\tCREATEUSER");
    if (detail.ReadBool(4)) sql << _T("\n\tCREATEDB");
    if (detail.ReadBool(5)) sql << _T("\n\tLOGIN");
    if (detail.ReadBool(6)) sql << _T("\n\tREPLICATION");
    int connectionLimit = detail.ReadInt4(7);
    if (connectionLimit >= 0)
      sql << _T("\n\tCONNECTION LIMIT ") << connectionLimit;
    wxString validUntil = detail[8];
    if (!validUntil.empty())
      sql << _T("\n\tVALID UNTIL ") << QuoteLiteral(validUntil);

    *output++ = sql;

    QueryResults memberships = Query(_T("Role Memberships")).OidParam(roloid).List();
    for (QueryResults::rows_iterator iter = memberships.Rows().begin(); iter != memberships.Rows().end(); iter++) {
      const QueryResults::Row& membership = *iter;
      sql = wxEmptyString;
      sql << _T("GRANT ") << membership[0] << _T(" TO ") << name;
      if (membership.ReadBool(2))
        sql << _T(" WITH ADMIN OPTION");
      *output++ = sql;
    }

    QueryResults dbSettingsSet = Query(_T("Role PerDatabase Settings")).OidParam(roloid).List();
    for (QueryResults::rows_iterator iter = dbSettingsSet.Rows().begin(); iter != dbSettingsSet.Rows().end(); iter++) {
      const QueryResults::Row& dbSettings = *iter;
      sql = wxEmptyString;
      sql << _T("ALTER ROLE ") << name;
      sql << _T(" IN DATABASE ") << dbSettings[0] << _T(' ');
      PgSettings(dbSettings[1]).GenerateSetStatements(output, this, sql);
    }
  }
    break;

  case Drop: {
    *output++ = _T("DROP ROLE ") + name;
  }
    break;

  default:
    wxASSERT(false);
  }
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
