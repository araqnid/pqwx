#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void TypeScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row detail = Query(_T("Type Detail")).OidParam(typoid).UniqueResult();
  wxString name = QuoteIdent(detail[0]);
  wxString schemaName = QuoteIdent(detail[1]);
  wxString owner = QuoteIdent(detail[2]);
  wxString qualifiedName = schemaName + _T('.') + name;

  switch (mode) {
  case Create: {
    wxString category = detail.ReadText(3);
    wxString description = detail.ReadText(4);
    if (category == _T("E")) {
      // enum
      wxString sql;
      sql << _T("CREATE TYPE ") << qualifiedName << _T(" AS ENUM (");
      QueryResults enumMembers = Query(_T("Enum Type Members")).OidParam(typoid).List();
      for (QueryResults::rows_iterator iter = enumMembers.Rows().begin(); iter != enumMembers.Rows().end(); iter++) {
        const QueryResults::Row& row = *iter;
        if (iter != enumMembers.Rows().begin()) sql << _T(", ");
        sql << QuoteLiteral(row[0]);
      }
      sql << _T(")");
      *output++ = sql;
    }
    else if (category == _T("C")) {
      // composite
      wxString sql;
      sql << _T("CREATE TYPE ") << qualifiedName << _T(" AS (");
      QueryResults attributes = Query(_T("Composite Type Attributes")).OidParam(typoid).List();
      for (QueryResults::rows_iterator iter = attributes.Rows().begin(); iter != attributes.Rows().end(); iter++) {
        const QueryResults::Row& row = *iter;
        wxString attname = QuoteIdent(row[0]);
        wxString atttype = row[1];
        wxString collationName = row[2];
        wxString collationSchema = row[3];
        if (iter != attributes.Rows().begin()) sql << _T(",");
        sql << _T("\n\t") << attname << _T(' ') << atttype;
        if (!collationName.empty()) {
          sql << _T(" COLLATE ");
          if (collationSchema != _T("pg_catalog"))
            sql << QuoteIdent(collationSchema) << _T('.') << QuoteIdent(collationName);
          else
            sql << QuoteIdent(collationName);
        }
      }
      sql << _T("\n)");
      *output++ = sql;
    }
    else {
      *output++ = _T("-- unkown type category: ") + QuoteLiteral(category);
    }
    if (!description.empty())
      *output++ = _T("COMMENT ON TYPE ") + qualifiedName + _T(" IS ") + QuoteLiteral(description);
  }
    break;
  case Alter: {
    wxString description = detail.ReadText(4);
    *output++ = _T("ALTER TYPE ") + qualifiedName + _T(" OWNER TO ") + owner;
    *output++ = _T("ALTER TYPE ") + qualifiedName + _T(" SET SCHEMA ") + schemaName;
    if (!description.empty())
      *output++ = _T("COMMENT ON TYPE ") + qualifiedName + _T(" IS ") + QuoteLiteral(description);
  }
    break;
  case Drop: {
    wxString sql;
    sql << _T("DROP TYPE /* IF EXISTS */ ")
        << qualifiedName
        << _T(" /* CASCADE */");
    *output++ = sql;
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
