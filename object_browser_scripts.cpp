#include "object_browser.h"
#include "object_browser_model.h"
#include "object_browser_database_work.h"

void DatabaseScriptWork::Execute() {
  QueryResults rs;
  DoQuery(_T("Database Detail"), rs, 26 /* oid */, database->oid);
  wxASSERT(rs.size() == 1);
  wxASSERT(rs[0].size() >= 5);
  wxString ownerName = rs[0][0];
  wxString encoding = rs[0][1];
  wxString collation = rs[0][2];
  wxString ctype = rs[0][3];
  long connectionLimit;
  rs[0][4].ToLong(&connectionLimit);

  wxString sql;
  switch (mode) {
  case Create:
    sql << _T("CREATE DATABASE ") << QuoteIdent(database->name);
    sql << _T("\n\tENCODING = ") << QuoteLiteral(encoding);
    sql << _T("\n\tLC_COLLATE = ") << QuoteLiteral(collation);
    sql << _T("\n\tLC_CTYPE = ") << QuoteLiteral(ctype);
    sql << _T("\n\tCONNECTION LIMIT = ") << connectionLimit;
    sql << _T("\n\tOWNER = ") << QuoteIdent(ownerName);
    break;

  case Alter:
    sql << _T("ALTER DATABASE ") << QuoteIdent(database->name);
    sql << _T("\n\tOWNER = ") << QuoteIdent(ownerName);
    sql << _T("\n\tCONNECTION LIMIT = ") << connectionLimit;
    break;

  case Drop:
    sql << _T("DROP DATABASE ") << QuoteIdent(database->name);
    break;

  default:
    wxASSERT(false);
  }

  statements.push_back(sql);
}

void TableScriptWork::Execute() {
  std::vector< wxString > tableDetail;
  QueryResults columns;
  DoQuery(_T("Table Detail"), tableDetail, 26 /* oid */, table->oid);
  DoQuery(_T("Relation Column Detail"), columns, 26 /* oid */, table->oid);

  switch (mode) {
  case Create: {
    wxString sql;
    std::vector< wxString > alterSql;
    sql << _T("CREATE TABLE ")
	<< QuoteIdent(table->schema) << _T(".") << QuoteIdent(table->name) << _T("(\n");
    unsigned n = 0;
    for (QueryResults::iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name(ReadText(iter, 0)),
	type(ReadText(iter, 1));
      sql << _T("\t") << QuoteIdent(name) << _T(" ") << type;

      bool notnull = ReadBool(iter, 2);
      if (notnull) sql << _T(" NOT NULL");

      bool hasdefault = ReadBool(iter, 3);
      if (hasdefault) {
	wxString defaultexpr(ReadText(iter, 4));
	sql << _T(" DEFAULT ") << defaultexpr;
      }

      wxString collation(ReadText(iter, 5));
      if (!collation.IsEmpty()) sql << _T(" COLLATE ") << collation; // already quoted

      long statsTarget(ReadInt4(iter, 6));
      if (statsTarget >= 0) {
	wxString statsSql;
	statsSql << _T("ALTER COLUMN ") << QuoteIdent(name)
		 << _T(" SET STATISTICS ") << statsTarget;
	alterSql.push_back(statsSql);
      }

      wxString storageType(ReadText(iter, 7));
      if (!storageType.IsEmpty()) {
	wxString storageSql;
	storageSql << _T("ALTER COLUMN ") << QuoteIdent(name)
		   << _T(" SET STORAGE ") << storageType;
	alterSql.push_back(storageSql);
      }

      if (n != (columns.size()-1)) sql << _T(',');
      sql << _T("\n");
    }
    sql << _T(")");
    statements.push_back(sql);
    if (!alterSql.empty()) {
      wxString prefix;
      prefix << _T("ALTER TABLE ")
	     << QuoteIdent(table->schema) << _T(".") << QuoteIdent(table->name) << _T("\n\t");
      for (std::vector<wxString>::iterator moreSqlIter = alterSql.begin(); moreSqlIter != alterSql.end(); moreSqlIter++) {
	statements.push_back(prefix + *moreSqlIter);
      }
    }
  }
    break;

  case Drop: {
    wxString sql;
    sql << _T("DROP TABLE IF EXISTS ")
	<< QuoteIdent(table->schema) << _T(".") << QuoteIdent(table->name);

    statements.push_back(sql);
  }
    break;

  case Select: {
    wxString sql;
    sql << _T("SELECT ");
    unsigned n = 0;
    for (QueryResults::iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name(ReadText(iter, 0));
      sql << QuoteIdent(name);
      if (n != (columns.size()-1))
	sql << _T(",\n       ");
      else
	sql << _T("\n");
    }
    sql << _T("FROM ") << QuoteIdent(table->schema) << _T(".") << QuoteIdent(table->name);
    statements.push_back(sql);
  }
    break;

  case Insert: {
    wxString sql;
    sql << _T("INSERT INTO ")
	<< QuoteIdent(table->schema) << _T(".") << QuoteIdent(table->name)
	<< _T("(\n");
    unsigned n = 0;
    for (QueryResults::iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name(ReadText(iter, 0));
      sql << _T("            ") << QuoteIdent(name);
      if (n != (columns.size()-1))
	sql << _T(",\n");
      else
	sql << _T("\n");
    }
    sql << _T(") VALUES (\n");
    n = 0;
    for (QueryResults::iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name(ReadText(iter, 0)), type(ReadText(iter, 1));
      sql << _T("            <") << QuoteIdent(name) << _T(", ") << type << _T(">");
      if (n != (columns.size()-1))
	sql << _T(",\n");
      else
	sql << _T("\n");
    }
    sql << _T(")");
    statements.push_back(sql);
  }
    break;

  case Update: {
    wxString sql;
    sql << _T("UPDATE ")
	<< QuoteIdent(table->schema) << _T(".") << QuoteIdent(table->name)
	<< _T("\nSET ");
    unsigned n = 0;
    for (QueryResults::iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name(ReadText(iter, 0)), type(ReadText(iter, 1));
      sql << QuoteIdent(name) << _T(" = ")
	  << _T("<") << QuoteIdent(name) << _T(", ") << type << _T(">");
      if (n != (columns.size()-1))
	sql << _T(",\n    ");
      else
	sql << _T("\n");
    }
    sql << _T("WHERE <") << _("search criteria") << _T(">");
    statements.push_back(sql);
  }
    break;

  case Delete: {
    wxString sql;
    sql << _T("DELETE FROM ")
	<< QuoteIdent(table->schema) << _T(".") << QuoteIdent(table->name);
    statements.push_back(sql);
  }
    break;

  default:
    wxASSERT(false);
  }
}

void ViewScriptWork::Execute() {
  std::vector< wxString > viewDetail;
  QueryResults columns;
  DoQuery(_T("View Detail"), viewDetail, 26 /* oid */, view->oid);
  DoQuery(_T("Relation Column Detail"), columns, 26 /* oid */, view->oid);

  switch (mode) {
  case Create:
  case Alter: {
    wxString definition = ReadText(viewDetail, 1);

    wxString sql;
    if (mode == Create)
      sql << _T("CREATE VIEW ");
    else
      sql << _T("CREATE OR REPLACE VIEW ");
    sql << QuoteIdent(view->schema) << _T(".") << QuoteIdent(view->name) << _T("(\n");
    unsigned n = 0;
    for (QueryResults::iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name(ReadText(iter, 0)), type(ReadText(iter, 1));
      sql << _T("\t") << QuoteIdent(name) << _T(" ") << type;
      if (n != (columns.size()-1))
	sql << _T(",\n");
    }
    sql << _T(") AS\n")
	<< definition;
    statements.push_back(sql);
  }
    break;

  case Select: {
    wxString sql;
    sql << _T("SELECT ");
    unsigned n = 0;
    for (QueryResults::iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name(ReadText(iter, 0));
      sql << QuoteIdent(name);
      if (n != (columns.size()-1))
	sql << _T(",\n       ");
      else
	sql << _T("\n");
    }
    sql << _T("FROM ") << QuoteIdent(view->schema) << _T(".") << QuoteIdent(view->name);
    statements.push_back(sql);
  }
    break;

  case Drop: {
    wxString sql;
    sql << _T("DROP VIEW IF EXISTS ")
	<< QuoteIdent(view->schema) << _T(".") << QuoteIdent(view->name);

    statements.push_back(sql);
  }
    break;

  default:
    wxASSERT(false);
  }
}

void SequenceScriptWork::Execute() {
  std::vector< wxString > sequenceDetail;
  DoQuery(_T("Sequence Detail"), sequenceDetail, 26 /* oid */, sequence->oid);
}

static void EscapeCode(const wxString &src, wxString &buf) {
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

static std::vector<Oid> ParseOidVector(const wxString &str) {
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

static inline std::vector<Oid> ReadOidVector(const std::vector<wxString> &row, unsigned index) {
  wxASSERT(index < row.size());
  return ParseOidVector(row[index]);
}

static std::vector<wxString> ParseTextArray(const wxString &str) {
  std::vector<wxString> result;
  if (str.IsEmpty()) return result;
  wxASSERT(str[0] == _T('{'));
  int state = 0;
  wxString buf;
  for (unsigned pos = 1; pos < str.length(); pos++) {
    wxChar c = str[pos];
    if (state == 0) {
      if (c == _T('}')) {
	result.push_back(buf);
	break;
      }
      else if (c == _T(',')) {
	result.push_back(buf);
	buf.Clear();
      }
      else if (c == _T('\"')) {
	state = 1;
      }
      else {
	buf << c;
      }
    }
    else if (state == 1) {
      if (c == _T('\"')) {
	state = 0;
      }
      if (c == _T('\"')) {
	state = 2;
      }
      else {
	buf << c;
      }
    }
    else if (state == 2) {
      buf << c;
      state = 1;
    }
  }
  return result;
}

static inline std::vector<wxString> ReadTextArray(const std::vector<wxString> &row, unsigned index) {
  wxASSERT(index < row.size());
  return ParseTextArray(row[index]);
}

static inline std::vector<Oid> ReadOidArray(const std::vector<wxString> &row, unsigned index) {
  wxASSERT(index < row.size());
  std::vector<wxString> strings = ParseTextArray(row[index]);
  std::vector<Oid> result;
  result.reserve(strings.size());
  for (std::vector<wxString>::const_iterator iter = strings.begin(); iter != strings.end(); iter++) {
    long value;
    if ((*iter).ToLong(&value)) {
      result.push_back((Oid) value);
    }
  }
  return result;
}

static inline std::vector<bool> ReadIOModeArray(const std::vector<wxString> &row, unsigned index) {
  wxASSERT(index < row.size());
  std::vector<wxString> strings = ParseTextArray(row[index]);
  std::vector<bool> result;
  result.reserve(strings.size());
  for (std::vector<wxString>::const_iterator iter = strings.begin(); iter != strings.end(); iter++) {
    wxASSERT_MSG((*iter) == _T("o") || (*iter) == _T("i"), *iter);
    result.push_back((*iter) == _T("o"));
  }
  return result;
}

std::map<Oid, FunctionScriptWork::Typeinfo> FunctionScriptWork::FetchTypes(const std::set<Oid> &types) {
  std::map<Oid, Typeinfo> typeMap;
  for (std::set<Oid>::const_iterator iter = types.begin(); iter != types.end(); iter++) {
    std::vector<wxString> typeInfo;
    DoQuery(_T("Type Info"), typeInfo, 26 /* oid */, *iter);
    Typeinfo typeinfo;
    typeinfo.schema = ReadText(typeInfo, 0);
    typeinfo.name = ReadText(typeInfo, 1);
    typeinfo.arrayDepth = ReadInt4(typeInfo, 2);
    typeMap[*iter] = typeinfo;
  }
  return typeMap;
}

template <class T>
static void DumpCollection(const wxString &tag, T collection) {
  typedef class T::const_iterator I;
  wxLogDebug(_T("%s:"), tag.c_str());
  unsigned n = 0;
  for (I iter = collection.begin(); iter != collection.end(); iter++, n++) {
    wxString line;
    line << _T(" [") << n << _T("] ") << *iter;
    wxLogDebug(_T("%s"), line.c_str());
  }
}

void FunctionScriptWork::Execute() {
  std::vector< wxString > functionDetail;
  DoQuery(_T("Function Detail"), functionDetail, 26 /* oid */, function->oid);
  std::vector<Oid> basicArgTypes = ReadOidVector(functionDetail, 1);
  std::vector<Oid> extendedArgTypes = ReadOidArray(functionDetail, 2);
  std::vector<bool> extendedArgModes = ReadIOModeArray(functionDetail, 3);
  std::vector<wxString> extendedArgNames = ReadTextArray(functionDetail, 4);
  std::map<Oid, Typeinfo> typeMap = FetchTypes(basicArgTypes, extendedArgTypes);

  switch (mode) {
  case Create:
  case Alter: {
    wxString sql;
    if (mode == Create)
      sql << _T("CREATE FUNCTION ");
    else
      sql << _T("CREATE OR REPLACE FUNCTION ");
    sql << QuoteIdent(function->schema) << _T(".") << QuoteIdent(function->name)
	<< _T("(");
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
    sql << _T(")")
	<< _T(" RETURNS ");

    if (ReadBool(functionDetail, 14)) sql << _T("SETOF ");
    sql << functionDetail[13];

    int cost = ReadInt4(functionDetail, 7);
    if (cost > 0) sql << _T(" COST ") << cost;

    int rows = ReadInt4(functionDetail, 8);
    if (rows > 0) sql << _T(" ROWS ") << rows;

    if (ReadBool(functionDetail, 9)) sql << _T(" SECURITY DEFINER ");

    if (ReadBool(functionDetail, 10)) sql << _T(" STRICT ");

    sql << _T(" ") << ReadText(functionDetail, 11); // volatility

    sql << _T(" AS ");
    EscapeCode(ReadText(functionDetail, 12), sql);
    statements.push_back(sql);
  }
    break;

  case Drop: {
    wxString sql;
    sql << _T("DROP FUNCTION IF EXISTS ")
	<< QuoteIdent(function->schema) << _T(".") << QuoteIdent(function->name)
	<< _T("(") << function->arguments << _T(")");
    statements.push_back(sql);
  }
    break;

  case Select: {
    wxString sql;
    Oid returnType = ReadOid(functionDetail, 0);

    if (returnType == 2249) {
      // "record" return type
      sql << _T("SELECT * FROM ");
    }
    else {
      // non-record return type
      sql << _T("SELECT ");
    }

    sql << QuoteIdent(function->schema) << _T('.') << QuoteIdent(function->name) << _T('(');
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

    statements.push_back(sql);
  }
    break;

  default:
    wxASSERT(false);
  }
}
