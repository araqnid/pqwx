#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void TextSearchDictionaryScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row detail = Query(_T("Text Search Dictionary Detail")).OidParam(dictoid).UniqueResult();
  wxString name = QuoteIdent(detail[0]);
  wxString schemaName = QuoteIdent(detail[1]);
  wxString owner = QuoteIdent(detail[2]);
  wxString templateName = QuoteIdent(detail[3]);
  wxString templateSchemaName = QuoteIdent(detail[4]);
  wxString options = detail[5];
  wxString qualifiedName = schemaName + _T('.') + name;
  wxString qualifiedTemplateName = templateSchemaName + _T('.') + templateName;
  switch (mode) {
  case Create: {
    wxString sql;
    sql << _T("CREATE TEXT SEARCH DICTIONARY ") << qualifiedName << _T("(\n");
    sql << _T("\tTEMPLATE = ") << qualifiedTemplateName;
    if (!options.empty())
      sql << _T(",\n\t") << options; // TODO parse and reformat?
    sql << _T("\n)");

    *output++ = sql;

    *output++ = _T("ALTER TEXT SEARCH DICTIONARY ") + qualifiedName + _T(" OWNER TO ") + owner;
  }
    break;

  case Alter: {
    if (!options.empty())
      *output++ = _T("ALTER TEXT SEARCH DICTIONARY (\n\t") + options + _T("\n)");
    *output++ = _T("ALTER TEXT SEARCH DICTIONARY ") + qualifiedName + _T(" RENAME TO ") + name;
    *output++ = _T("ALTER TEXT SEARCH DICTIONARY ") + qualifiedName + _T(" OWNER TO ") + owner;
    *output++ = _T("ALTER TEXT SEARCH DICTIONARY ") + qualifiedName + _T(" SET SCHEMA ") + schemaName;
  }
    break;

  case Drop: {
    *output++ = _T("DROP TEXT SEARCH DICTIONARY IF EXISTS ") + qualifiedName;
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
