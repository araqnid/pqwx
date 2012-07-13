#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void TextSearchTemplateScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row detail = Query(_T("Text Search Template Detail")).OidParam(tmploid).UniqueResult();
  wxString name = QuoteIdent(detail[0]);
  wxString schemaName = QuoteIdent(detail[1]);
  wxString initFunction = QuoteIdent(detail[2]);
  wxString lexizeFunction = QuoteIdent(detail[3]);
  wxString qualifiedName = schemaName + _T('.') + name;
  switch (mode) {
  case Create: {
    wxString sql;
    sql << _T("CREATE TEXT SEARCH TEMPLATE ") << qualifiedName << _T("(");
    if (!initFunction.empty())
      sql << _T("\n\tINIT = ") << initFunction << _T(',');
    sql << _T("\n\tLEXIZE = ") << lexizeFunction;
    sql << _T("\n)");

    *output++ = sql;
  }
    break;

  case Alter: {
    *output++ = _T("ALTER TEXT SEARCH TEMPLATE ") + qualifiedName + _T(" RENAME TO ") + name;
    *output++ = _T("ALTER TEXT SEARCH TEMPLATE ") + qualifiedName + _T(" SET SCHEMA ") + schemaName;
  }
    break;

  case Drop: {
    *output++ = _T("DROP TEXT SEARCH TEMPLATE IF EXISTS ") + qualifiedName;
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
