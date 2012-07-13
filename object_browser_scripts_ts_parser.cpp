#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void TextSearchParserScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row detail = Query(_T("Text Search Parser Detail")).OidParam(prsoid).UniqueResult();
  wxString name = QuoteIdent(detail[0]);
  wxString schemaName = QuoteIdent(detail[1]);
  wxString startFunction = QuoteIdent(detail[2]);
  wxString tokenFunction = QuoteIdent(detail[3]);
  wxString endFunction = QuoteIdent(detail[4]);
  wxString lextypeFunction = QuoteIdent(detail[5]);
  wxString headlineFunction = QuoteIdent(detail[6]);
  wxString qualifiedName = schemaName + _T('.') + name;
  switch (mode) {
  case Create: {
    wxString sql;
    sql << _T("CREATE TEXT SEARCH PARSER ") << qualifiedName << _T("(");
    sql << _T("\n\tSTART = ") << startFunction << _T(',');
    sql << _T("\n\tGETTOKEN = ") << tokenFunction << _T(',');
    sql << _T("\n\tEND = ") << endFunction << _T(',');
    sql << _T("\n\tLEXTYPES = ") << lextypeFunction;
    if (!headlineFunction.empty())
      sql << _T(",\n\tHEADLINE = ") << headlineFunction;
    sql << _T("\n)");

    *output++ = sql;
  }
    break;

  case Alter: {
    *output++ = _T("ALTER TEXT SEARCH PARSER ") + qualifiedName + _T(" RENAME TO ") + name;
    *output++ = _T("ALTER TEXT SEARCH PARSER ") + qualifiedName + _T(" SET SCHEMA ") + schemaName;
  }
    break;

  case Drop: {
    *output++ = _T("DROP TEXT SEARCH PARSER IF EXISTS ") + qualifiedName;
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
