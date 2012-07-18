#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void TableScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row tableDetail = Query(_T("Table Detail")).OidParam(reloid).UniqueResult();
  QueryResults columns = Query(_T("Relation Column Detail")).OidParam(reloid).List();
  QueryResults foreignKeys = Query(_T("Table Foreign Keys")).OidParam(reloid).List();
  wxString tableName = QuoteIdent(tableDetail[1]) + _T('.') + QuoteIdent(tableDetail[0]);

  switch (mode) {
  case Create: {
    wxString sql;
    std::vector< wxString > alterSql;
    bool unlogged = tableDetail.ReadBool(8);
    sql << (unlogged ? _T("CREATE UNLOGGED TABLE ") : _T("CREATE TABLE ")) << tableName << _T("(\n");
    unsigned n = 0;
    for (QueryResults::rows_iterator iter = columns.Rows().begin(); iter != columns.Rows().end(); iter++, n++) {
      wxString name((*iter).ReadText(0)),
        type((*iter).ReadText(1));
      sql << _T("\t") << QuoteIdent(name) << _T(" ") << type;

      bool notnull = (*iter).ReadBool(2);
      if (notnull) sql << _T(" NOT NULL");

      bool hasdefault = (*iter).ReadBool(3);
      if (hasdefault) {
        wxString defaultexpr((*iter).ReadText(4));
        sql << _T(" DEFAULT ") << defaultexpr;
      }

      wxString collation((*iter).ReadText(5));
      if (!collation.IsEmpty()) sql << _T(" COLLATE ") << collation; // already quoted

      long statsTarget((*iter).ReadInt4(6));
      if (statsTarget >= 0) {
        wxString statsSql;
        statsSql << _T("ALTER COLUMN ") << QuoteIdent(name)
                 << _T(" SET STATISTICS ") << statsTarget;
        alterSql.push_back(statsSql);
      }

      wxString storageType((*iter).ReadText(7));
      if (!storageType.IsEmpty()) {
        wxString storageSql;
        storageSql << _T("ALTER COLUMN ") << QuoteIdent(name)
                   << _T(" SET STORAGE ") << storageType;
        alterSql.push_back(storageSql);
      }

      if (n != (columns.Rows().size()-1)) sql << _T(',');
      sql << _T("\n");
    }
    sql << _T(")");
    *output++ = sql;
    if (!alterSql.empty()) {
      wxString prefix;
      prefix << _T("ALTER TABLE ") << tableName << _T("\n\t");
      for (std::vector<wxString>::iterator moreSqlIter = alterSql.begin(); moreSqlIter != alterSql.end(); moreSqlIter++) {
        *output++ = prefix + *moreSqlIter;
      }
    }

    PgAcl(tableDetail[5]).GenerateGrantStatements(output, tableDetail[2], tableName, privilegeMap);
    PgSettings(tableDetail[6]).GenerateSetStatements(output, this, _T("ALTER TABLE ") + tableName + _T(' '), true);

    AddDescription(output, _T("TABLE"), tableName, tableDetail[7]);

    for (QueryResults::rows_iterator iter = columns.Rows().begin(); iter != columns.Rows().end(); iter++) {
      wxString description = (*iter).ReadText(10);
      if (!description.empty()) {
        *output++ = _T("COMMENT ON COLUMN ") + tableName + _T('.') + (*iter).ReadText(0) + _T(" IS ") + QuoteLiteral(description);
      }
    }

    if (foreignKeys.Rows().size() > 0) {
      wxString lastKeyName;
      wxString srcColumns;
      wxString dstColumns;
      const QueryResults::Row *lastKey = NULL;
      for (QueryResults::rows_iterator iter = foreignKeys.Rows().begin(); iter != foreignKeys.Rows().end(); iter++) {
        wxString fkeyName = (*iter)[_T("conname")];
        if (!lastKeyName.IsEmpty() && fkeyName != lastKeyName) {
          GenerateForeignKey(output, tableName, srcColumns, dstColumns, *lastKey);
          srcColumns = wxEmptyString;
          dstColumns = wxEmptyString;
        }
        lastKeyName = fkeyName;
        lastKey = &(*iter);
        if (!srcColumns.IsEmpty()) srcColumns += _T(",");
        srcColumns += QuoteIdent((*iter)[_T("srcatt")]);
        if (!dstColumns.IsEmpty()) dstColumns += _T(",");
        dstColumns += QuoteIdent((*iter)[_T("dstatt")]);
      }
      GenerateForeignKey(output, tableName, srcColumns, dstColumns, *lastKey);
    }
  }
    break;

  case Drop: {
    wxString sql;
    sql << _T("DROP TABLE IF EXISTS ") << tableName;

    *output++ = sql;
  }
    break;

  case Select: {
    wxString sql;
    sql << _T("SELECT ");
    unsigned n = 0;
    for (QueryResults::rows_iterator iter = columns.Rows().begin(); iter != columns.Rows().end(); iter++, n++) {
      wxString name((*iter).ReadText(0));
      sql << QuoteIdent(name);
      if (n != (columns.Rows().size()-1))
        sql << _T(",\n       ");
      else
        sql << _T("\n");
    }
    sql << _T("FROM ") << tableName;
    *output++ = sql;
  }
    break;

  case Insert: {
    wxString sql;
    sql << _T("INSERT INTO ") << tableName << _T("(\n");
    unsigned n = 0;
    for (QueryResults::rows_iterator iter = columns.Rows().begin(); iter != columns.Rows().end(); iter++, n++) {
      wxString name((*iter).ReadText(0));
      sql << _T("            ") << QuoteIdent(name);
      if (n != (columns.Rows().size()-1))
        sql << _T(",\n");
      else
        sql << _T("\n");
    }
    sql << _T(") VALUES (\n");
    n = 0;
    for (QueryResults::rows_iterator iter = columns.Rows().begin(); iter != columns.Rows().end(); iter++, n++) {
      wxString name((*iter).ReadText(0)), type((*iter).ReadText(1));
      sql << _T("            <") << QuoteIdent(name) << _T(", ") << type << _T(">");
      if (n != (columns.Rows().size()-1))
        sql << _T(",\n");
      else
        sql << _T("\n");
    }
    sql << _T(")");
    *output++ = sql;
  }
    break;

  case Update: {
    wxString sql;
    sql << _T("UPDATE ") << tableName << _T("\nSET ");
    unsigned n = 0;
    for (QueryResults::rows_iterator iter = columns.Rows().begin(); iter != columns.Rows().end(); iter++, n++) {
      wxString name((*iter).ReadText(0)), type((*iter).ReadText(1));
      sql << QuoteIdent(name) << _T(" = ")
          << _T("<") << QuoteIdent(name) << _T(", ") << type << _T(">");
      if (n != (columns.Rows().size()-1))
        sql << _T(",\n    ");
      else
        sql << _T("\n");
    }
    sql << _T("WHERE <") << _("search criteria") << _T(">");
    *output++ = sql;
  }
    break;

  case Delete: {
    wxString sql;
    sql << _T("DELETE FROM ") << tableName;
    *output++ = sql;
  }
    break;

  default:
    wxASSERT(false);
  }
}

void TableScriptWork::GenerateForeignKey(OutputIterator output, const wxString& tableName, const wxString& srcColumns, const wxString& dstColumns, const QueryResults::Row& fkeyRow)
{
  wxString fkeySql =_T("ALTER TABLE ") + tableName +
    _T(" ADD CONSTRAINT ") + QuoteIdent(fkeyRow[_T("conname")]) +
    _T("\n\tFOREIGN KEY (") + srcColumns +
    _T(")\n\tREFERENCES ") + QuoteIdent(fkeyRow[_T("dstnspname")]) + _T(".") + QuoteIdent(fkeyRow[_T("dstrelname")]) +
    _T("(") + dstColumns + _T(")");

  wxString confupdtype = fkeyRow[_T("confupdtype")];
  if (confupdtype == _T("r")) {
    fkeySql += _T("\n\tON UPDATE RESTRICT");
  }
  else if (confupdtype == _T("c")) {
    fkeySql += _T("\n\tON UPDATE CASCADE");
  }
  else if (confupdtype == _T("n")) {
    fkeySql += _T("\n\tON UPDATE SET NULL");
  }
  else if (confupdtype == _T("d")) {
    fkeySql += _T("\n\tON UPDATE SET DEFAULT");
  }

  wxString confdeltype = fkeyRow[_T("confdeltype")];
  if (confdeltype == _T("r")) {
    fkeySql += _T("\n\tON DELETE RESTRICT");
  }
  else if (confdeltype == _T("c")) {
    fkeySql += _T("\n\tON DELETE CASCADE");
  }
  else if (confdeltype == _T("n")) {
    fkeySql += _T("\n\tON DELETE SET NULL");
  }
  else if (confdeltype == _T("d")) {
    fkeySql += _T("\n\tON DELETE SET DEFAULT");
  }

  wxString confmatchtype = fkeyRow[_T("confmatchtype")];
  if (confmatchtype == _T("f")) {
    fkeySql += _T("\n\tMATCH FULL");
  }

  *output++ = fkeySql;
}

std::map<wxChar, wxString> TableScriptWork::privilegeMap = PrivilegeMap(_T("a=INSERT r=SELECT w=UPDATE d=DELETE D=TRUNCATE x=REFERENCES t=TRIGGER"));

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
