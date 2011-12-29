#include "object_browser.h"
#include "object_browser_model.h"
#include "object_browser_database_work.h"

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
    int n = 0;
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
    int n = 0;
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
    int n = 0;
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
    int n = 0;
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
    wxString sql;
    if (mode == Create)
      sql << _T("CREATE VIEW ");
    else
      sql << _T("CREATE OR REPLACE VIEW ");
    sql << QuoteIdent(view->schema) << _T(".") << QuoteIdent(view->name) << _T("(\n");
    int n = 0;
    for (QueryResults::iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name(ReadText(iter, 0)), type(ReadText(iter, 1));
      sql << _T("\t") << QuoteIdent(name) << _T(" ") << type;
      if (n != (columns.size()-1))
	sql << _T(",\n");
    }
    sql << _T(") AS\n")
	<< viewDetail[1];
    statements.push_back(sql);
  }
    break;

  case Select: {
    wxString sql;
    sql << _T("SELECT ");
    int n = 0;
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

static  void EscapeCode(const wxString &src, wxString &buf) {
  if (src.Find(_T("$$")) == wxNOT_FOUND) {
    buf << _T("$$") << src << _T("$$");
    return;
  }
  if (src.Find(_T("$_$")) == wxNOT_FOUND) {
    buf << _T("$_$") << src << _T("$_$");
    return;
  }
  int n = 0;
  while (1) {
    wxString delimiter;
    delimiter << _T('$') << n << _T('$');
    if (src.Find(delimiter) == wxNOT_FOUND) {
      buf << delimiter << src << delimiter;
    }
    ++n;
  }
}

void FunctionScriptWork::Execute() {
  std::vector< wxString > functionDetail;
  DoQuery(_T("Function Detail"), functionDetail, 26 /* oid */, function->oid);

  switch (mode) {
  case Create:
  case Alter: {
    wxString sql;
    if (mode == Create)
      sql << _T("CREATE FUNCTION ");
    else
      sql << _T("CREATE OR REPLACE FUNCTION ");
    sql << QuoteIdent(function->schema) << _T(".") << QuoteIdent(function->name)
	<< _T("(") << functionDetail[0] << _T(")")
	<< _T(" RETURNS ");

    bool rowset;
    rowset = functionDetail[10].IsSameAs(_T("t"));
    if (rowset) sql << _T("SETOF ");
    sql << functionDetail[9];

    long cost;
    functionDetail[3].ToLong(&cost);
    if (cost > 0)
      sql << _T(" COST ") << cost;

    long rows;
    functionDetail[4].ToLong(&rows);
    if (rows > 0)
      sql << _T(" ROWS ") << rows;

    bool securityDefiner;
    securityDefiner = functionDetail[5].IsSameAs(_T("t"));
    if (securityDefiner)
      sql << _T(" SECURITY DEFINER ");

    bool isStrict;
    isStrict = functionDetail[6].IsSameAs(_T("t"));
    if (isStrict)
      sql << _T(" STRICT ");

    sql << _T(" ") << functionDetail[7]; // volatility

    sql << _T(" AS ");
    EscapeCode(functionDetail[8], sql);
    statements.push_back(sql);
  }
    break;

  case Drop: {
    wxString sql;
    sql << _T("DROP FUNCTION IF EXISTS ")
	<< QuoteIdent(function->schema) << _T(".") << QuoteIdent(function->name)
	<< _T("(") << functionDetail[0] << _T(")");
    statements.push_back(sql);
  }
    break;

  }
}
