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

void DatabaseScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row row = Query(_T("Database Detail")).OidParam(dboid).UniqueResult();
  wxASSERT(row.size() >= 6);
  wxString databaseName = row[0];
  wxString ownerName = row[1];
  wxString encoding = row[2];
  wxString collation = row[3];
  wxString ctype = row[4];
  long connectionLimit;
  row[4].ToLong(&connectionLimit);

  wxString sql;
  switch (mode) {
  case Create:
    sql << _T("CREATE DATABASE ") << QuoteIdent(databaseName);
    sql << _T("\n\tENCODING = ") << QuoteLiteral(encoding);
    sql << _T("\n\tLC_COLLATE = ") << QuoteLiteral(collation);
    sql << _T("\n\tLC_CTYPE = ") << QuoteLiteral(ctype);
    sql << _T("\n\tCONNECTION LIMIT = ") << connectionLimit;
    sql << _T("\n\tOWNER = ") << QuoteIdent(ownerName);
    break;

  case Alter:
    sql << _T("ALTER DATABASE ") << QuoteIdent(databaseName);
    sql << _T("\n\tOWNER = ") << QuoteIdent(ownerName);
    sql << _T("\n\tCONNECTION LIMIT = ") << connectionLimit;
    break;

  case Drop:
    sql << _T("DROP DATABASE ") << QuoteIdent(databaseName);
    break;

  default:
    wxASSERT(false);
  }

  *output++ = sql;

  if (mode != Drop) {
    PgAcl(row[6]).GenerateGrantStatements(output, ownerName, _T("DATABASE ") + QuoteIdent(databaseName), privilegeMap);
    QueryResults settingsRs = Query(_T("Database Settings")).OidParam(dboid).List();
    for (unsigned i = 0; i < settingsRs.size(); i++) {
      wxString role = settingsRs[i][0];
      wxString prefix;
      if (role.empty())
        prefix << _T("ALTER DATABASE ") << databaseName << _T(' ');
      else
        prefix << _T("ALTER ROLE ") << role << _T(" IN DATABASE ") << databaseName << _T(' ');
      PgSettings(settingsRs[i][1]).GenerateSetStatements(output, this, prefix);
    }

    AddDescription(output, _T("DATABASE"), databaseName, row[7]);
  }
}

std::map<wxChar, wxString> DatabaseScriptWork::privilegeMap = PrivilegeMap(_T("C=CREATE T=TEMP c=CONNECT"));

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
