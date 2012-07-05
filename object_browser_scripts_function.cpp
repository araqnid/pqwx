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

void FunctionScriptWork::EscapeCode(const wxString &src, wxString &buf)
{
  if (src.Find(_T("$$")) == wxNOT_FOUND) {
    buf << _T("$$") << src << _T("$$");
    return;
  }
  if (src.Find(_T("$_$")) == wxNOT_FOUND) {
    buf << _T("$_$") << src << _T("$_$");
    return;
  }
  unsigned n = 0;
  while (1) {
    wxString delimiter;
    delimiter << _T('$') << n << _T('$');
    if (src.Find(delimiter) == wxNOT_FOUND) {
      buf << delimiter << src << delimiter;
    }
    ++n;
  }
}

std::map<wxChar, wxString> FunctionScriptWork::privilegeMap = PrivilegeMap(_T("X=EXECUTE"));

std::vector<Oid> ScriptWork::ParseOidVector(const wxString &str)
{
  std::vector<Oid> result;
  unsigned mark = 0;
  for (unsigned pos = 0; pos < str.length(); pos++) {
    if (str[pos] == ' ') {
      long typeOid;
      if (str.Mid(mark, pos-mark).ToLong(&typeOid)) {
        result.push_back((Oid) typeOid);
      }
      mark = pos + 1;
    }
  }

  if (mark < str.length()) {
    long typeOid;
    if (str.Mid(mark, str.length()-mark).ToLong(&typeOid)) {
      result.push_back((Oid) typeOid);
    }
  }

  return result;
}

std::vector<bool> FunctionScriptWork::ReadIOModeArray(const QueryResults::Row &row, unsigned index)
{
  wxASSERT(index < row.size());
  std::vector<wxString> strings = ReadTextArray(row, index);
  std::vector<bool> result;
  result.reserve(strings.size());
  for (std::vector<wxString>::const_iterator iter = strings.begin(); iter != strings.end(); iter++) {
    wxASSERT_MSG((*iter) == _T("o") || (*iter) == _T("i"), *iter);
    result.push_back((*iter) == _T("o"));
  }
  return result;
}

std::map<Oid, FunctionScriptWork::Typeinfo> FunctionScriptWork::FetchTypes(const std::set<Oid> &types)
{
  std::map<Oid, Typeinfo> typeMap;
  for (std::set<Oid>::const_iterator iter = types.begin(); iter != types.end(); iter++) {
    QueryResults::Row typeInfo = Query(_T("Type Info")).OidParam(*iter).UniqueResult();
    Typeinfo typeinfo;
    typeinfo.schema = typeInfo.ReadText(0);
    typeinfo.name = typeInfo.ReadText(1);
    typeinfo.arrayDepth = typeInfo.ReadInt4(2);
    typeMap[*iter] = typeinfo;
  }
  return typeMap;
}

void FunctionScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row functionDetail = Query(_T("Function Detail")).OidParam(procoid).UniqueResult();
  wxString functionCoreName = QuoteIdent(functionDetail[1]) + _T('.') + QuoteIdent(functionDetail[0]);
  wxString functionName = functionCoreName + _T('(') + functionDetail[2] + _T(')');
  std::vector<Oid> basicArgTypes = ReadOidVector(functionDetail, 4);
  std::vector<Oid> extendedArgTypes = ReadOidArray(functionDetail, 5);
  std::vector<bool> extendedArgModes = ReadIOModeArray(functionDetail, 6);
  std::vector<wxString> extendedArgNames = ReadTextArray(functionDetail, 7);
  std::map<Oid, Typeinfo> typeMap = FetchTypes(basicArgTypes, extendedArgTypes);
  bool aggregate = functionDetail.ReadBool(21);

  switch (mode) {
  case Create:
  case Alter: {
    wxString sql;
    if (mode == Create)
      sql << _T("CREATE ");
    else
      sql << _T("CREATE OR REPLACE ");
    if (aggregate)
      sql << _T("AGGREGATE ");
    else
      sql << _T("FUNCTION ");
    sql << functionCoreName << _T("(");
    if (!extendedArgTypes.empty()) {
      unsigned pos = 0;
      for (std::vector<Oid>::const_iterator iter = extendedArgTypes.begin(); iter != extendedArgTypes.end(); iter++, pos++) {
        wxASSERT_MSG(typeMap.count(*iter) > 0, wxString::Format(_T("OID %u not in typeMap"), *iter));
        Typeinfo typeinfo = typeMap[*iter];
        if (pos < extendedArgNames.size())
          sql << QuoteIdent(extendedArgNames[pos]) << _T(' ');
        if (extendedArgModes[pos])
          sql << _T("OUT ");
        if (typeinfo.schema == _T("pg_catalog"))
          sql << QuoteIdent(typeinfo.name);
        else
          sql << QuoteIdent(typeinfo.schema) << _T('.') << QuoteIdent(typeinfo.name);
        for (int i = 0; i < typeinfo.arrayDepth; i++) sql << _T("[]");
        if (pos != (extendedArgTypes.size() - 1)) sql << _T(", ");
      }
    }
    else {
      unsigned pos = 0;
      for (std::vector<Oid>::const_iterator iter = basicArgTypes.begin(); iter != basicArgTypes.end(); iter++, pos++) {
        wxASSERT_MSG(typeMap.count(*iter) > 0, wxString::Format(_T("OID %u not in typeMap"), *iter));
        Typeinfo typeinfo = typeMap[*iter];
        if (pos < extendedArgNames.size())
          sql << QuoteIdent(extendedArgNames[pos]) << _T(' ');
        if (typeinfo.schema == _T("pg_catalog"))
          sql << QuoteIdent(typeinfo.name);
        else
          sql << QuoteIdent(typeinfo.schema) << _T('.') << QuoteIdent(typeinfo.name);
        for (int i = 0; i < typeinfo.arrayDepth; i++) sql << _T("[]");
        if (pos != (basicArgTypes.size() - 1)) sql << _T(", ");
      }
    }
    sql << _T(")");

    if (!aggregate) {
      sql << _T(" RETURNS ");

      if (functionDetail.ReadBool(17)) sql << _T("SETOF ");
      sql << functionDetail[16];

      int cost = functionDetail.ReadInt4(10);
      if (cost > 0) sql << _T(" COST ") << cost;

      int rows = functionDetail.ReadInt4(11);
      if (rows > 0) sql << _T(" ROWS ") << rows;

      if (functionDetail.ReadBool(12)) sql << _T(" SECURITY DEFINER ");

      if (functionDetail.ReadBool(13)) sql << _T(" STRICT ");

      sql << _T(" ") << functionDetail.ReadText(14); // volatility

      sql << _T(" AS ");
      EscapeCode(functionDetail.ReadText(15), sql);
    }
    else {
      sql << _T(" (sfunc = ") << functionDetail.ReadText(22)
          << _T(", stype = ") << functionDetail.ReadText(23);
      wxString finalfunc = functionDetail.ReadText(24);
      if (!finalfunc.empty())
        sql << _T(", finalfunc = ") << finalfunc;
      wxString initval = functionDetail.ReadText(25);
      if (!initval.empty())
        sql << _T(", initcond = ") << initval;
      wxString sortop = functionDetail.ReadText(26);
      if (!sortop.empty())
        sql << _T(", sortop = operator(") << sortop << _T(")");
      sql << _T(")");
    }

    *output++ = sql;

    PgAcl(functionDetail[19]).GenerateGrantStatements(output, functionDetail[8], _T("FUNCTION ") + functionName, privilegeMap);
    PgSettings(functionDetail[18]).GenerateSetStatements(output, this, _T("ALTER FUNCTION ") + functionName + _T(' '));

    AddDescription(output, _T("FUNCTION"), functionName, functionDetail[20]);
  }
    break;

  case Drop: {
    wxString sql;
    sql << _T("DROP");
    if (aggregate)
      sql << _T(" AGGREGATE");
    else
      sql << _T(" FUNCTION");
    sql << _T(" IF EXISTS ") << functionName;
    *output++ = sql;
  }
    break;

  case Select: {
    wxString sql;
    Oid returnType = functionDetail.ReadOid(3);

    if (returnType == 2249) {
      // "record" return type
      sql << _T("SELECT * FROM ");
    }
    else {
      // non-record return type
      sql << _T("SELECT ");
    }

    sql << functionCoreName << _T('(');
    if (!basicArgTypes.empty()) {
      unsigned pos = 0;
      for (std::vector<Oid>::const_iterator iter = basicArgTypes.begin(); iter != basicArgTypes.end(); iter++, pos++) {
        Typeinfo typeinfo = typeMap[*iter];
        sql << _T('<');
        if (pos < extendedArgNames.size())
          sql << QuoteIdent(extendedArgNames[pos]);
        else
          sql << _("Argument ") << pos;
        if (typeinfo.schema == _T("pg_catalog"))
          sql << QuoteIdent(typeinfo.name);
        else
          sql << QuoteIdent(typeinfo.schema) << _T('.') << QuoteIdent(typeinfo.name);
        for (int i = 0; i < typeinfo.arrayDepth; i++) sql << _T("[]");
        sql << _T('>');
        if (pos != (basicArgTypes.size() - 1))
          sql << _T(",\n    ");
        else if (basicArgTypes.size() > 1)
          sql << _T("\n");
      }
    }
    sql << _T(')');

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
