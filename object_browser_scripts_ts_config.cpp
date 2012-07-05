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

void TextSearchConfigurationScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row detail = Query(_T("Text Search Configuration Detail")).OidParam(cfgoid).UniqueResult();
  wxString name = QuoteIdent(detail[0]);
  wxString schemaName = QuoteIdent(detail[1]);
  wxString owner = QuoteIdent(detail[2]);
  wxString parserName = QuoteIdent(detail[3]);
  wxString parserSchemaName = QuoteIdent(detail[4]);
  qualifiedName = schemaName + _T('.') + name;
  wxString qualifiedParserName = parserSchemaName + _T('.') + parserName;

  switch (mode) {
  case Create: {
    wxString sql;
    sql << _T("CREATE TEXT SEARCH CONFIGURATION ") << qualifiedName << _T("(");
    sql << _T("\n\tPARSER = ") << qualifiedParserName;
    sql << _T("\n)");

    *output++ = sql;
    GenerateMappings(output);
    *output++ = _T("ALTER TEXT SEARCH CONFIGURATION ") + qualifiedName + _T(" OWNER TO ") + owner;
  }
    break;

  case Alter: {
    *output++ = _T("ALTER TEXT SEARCH CONFIGURATION ") + qualifiedName + _T(" RENAME TO ") + name;
    *output++ = _T("ALTER TEXT SEARCH CONFIGURATION ") + qualifiedName + _T(" SET SCHEMA ") + schemaName;
    *output++ = _T("ALTER TEXT SEARCH CONFIGURATION ") + qualifiedName + _T(" OWNER TO ") + owner;
    GenerateMappings(output);
  }
    break;

  case Drop: {
    *output++ = _T("DROP TEXT SEARCH CONFIGURATION IF EXISTS ") + qualifiedName;
  }
    break;

  default:
    wxASSERT(false);
  }
}

void TextSearchConfigurationScriptWork::GenerateMappings(OutputIterator output)
{
  QueryResults mapping = Query(_T("Text Search Configuration Mappings")).OidParam(cfgoid).List();
  wxString mappingSql;
  int lastTokenType = -1;
  for (unsigned i = 0; i < mapping.size(); i++) {
    int tokenType = mapping[i].ReadInt4(0);
    wxString dictName = QuoteIdent(mapping[i].ReadText(1));
    wxString dictSchemaName = QuoteIdent(mapping[i].ReadText(2));
    if (tokenType != lastTokenType) {
      if (!mappingSql.empty()) {
        *output++ = mappingSql;
        mappingSql = wxEmptyString;
      }
      lastTokenType = tokenType;
    }
    if (mappingSql.empty()) {
      wxString tokenTypeName;
      if ((unsigned) tokenType < tokenTypeAliases.size())
        tokenTypeName = tokenTypeAliases[tokenType];
      else
        tokenTypeName << tokenType;
      mappingSql << _T("ALTER TEXT SEARCH CONFIGURATION ") << qualifiedName
                 << _T("\n\tADD MAPPING FOR ") << QuoteIdent(tokenTypeName)
                 << _T(" WITH ") << dictSchemaName << _T('.') << dictName;
    }
    else {
      mappingSql << _T(", ") << dictSchemaName << _T('.') << dictName;
    }
  }
  if (!mappingSql.empty()) *output++ = mappingSql;

}

static std::vector<wxString> TokenTypeAliases()
{
  std::vector<wxString> result;
  result.push_back(_T(""));
  result.push_back(_T("asciiword"));
  result.push_back(_T("word"));
  result.push_back(_T("numword"));
  result.push_back(_T("email"));
  result.push_back(_T("url"));
  result.push_back(_T("host"));
  result.push_back(_T("sfloat"));
  result.push_back(_T("version"));
  result.push_back(_T("hword_numpart"));
  result.push_back(_T("hword_part"));
  result.push_back(_T("hword_asciipart"));
  result.push_back(_T("blank"));
  result.push_back(_T("tag"));
  result.push_back(_T("protocol"));
  result.push_back(_T("numhword"));
  result.push_back(_T("asciihword"));
  result.push_back(_T("hword"));
  result.push_back(_T("url_path"));
  result.push_back(_T("file"));
  result.push_back(_T("float"));
  result.push_back(_T("int"));
  result.push_back(_T("uint"));
  result.push_back(_T("entity"));
  return result;
}

const std::vector<wxString> TextSearchConfigurationScriptWork::tokenTypeAliases = TokenTypeAliases();

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
