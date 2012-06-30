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

#include "wx/tokenzr.h"
#include "wx/regex.h"
#include "wx/clipbrd.h"

void ScriptWork::UpdateView(ObjectBrowser *ob)
{
  switch (output) {
  case Window: {
    const DatabaseModel *database = ob->objectBrowserModel->FindDatabase(databaseRef);
    PQWXDatabaseEvent evt(database->server->conninfo, database->name, PQWX_ScriptToWindow);
    evt.SetString(script);
    ob->ProcessEvent(evt);
    return;
  }
    break;

  case File:
    wxMessageBox(_T("TODO"));
    break;

  case Clipboard:
    if (wxTheClipboard->Open()) {
      wxTheClipboard->SetData(new wxTextDataObject(script));
      wxTheClipboard->Close();
    }
    break;
  default:
    wxASSERT(false);
  }
}

std::map<wxChar, wxString> ScriptWork::PrivilegeMap(const wxString &spec)
{
  std::map<wxChar, wxString> privilegeMap;
  wxStringTokenizer tkz(spec, _T(", \n\t"));
  while (tkz.HasMoreTokens()) {
    wxString token = tkz.GetNextToken();
    wxASSERT(token.length() > 2);
    privilegeMap[token[0]] = token.Mid(2);
  }
  return privilegeMap;
}

class PgAcl {
public:
  PgAcl(const wxString& input)
  {
    if (!input.empty()) {
      wxASSERT(input[0] == _T('{'));
      wxASSERT(input[input.length()-1] == _T('}'));
      wxStringTokenizer tkz(input.Mid(1, input.length()-2), _T(","));
      while (tkz.HasMoreTokens()) {
        entries.push_back(AclEntry(tkz.GetNextToken()));
      }
    }
  }

  template <class OutputIterator>
  void GenerateGrantStatements(OutputIterator output, const wxString &owner, const wxString &entity, const std::map<wxChar, wxString> &privilegeNames) const
  {
    for (std::vector<AclEntry>::const_iterator iter = entries.begin(); iter != entries.end(); iter++) {
      if ((*iter).grantee != owner) {
        (*iter).GenerateGrantStatement(output, entity, privilegeNames, true);
        (*iter).GenerateGrantStatement(output, entity, privilegeNames, false);
      }
    }
  }

  class InvalidAclEntry : public std::exception {
  public:
    InvalidAclEntry(const wxString &input) : input(input) {}
    ~InvalidAclEntry() throw () {}
    const char *what() { return "Invalid ACL entry"; }
    const wxString& GetInput() const { return input; }
  private:
    wxString input;
  };

private:
  class AclEntry {
  public:
    AclEntry(const wxString &input)
    {
      if (!pattern.Matches(input)) throw InvalidAclEntry(input);
      grantee = pattern.GetMatch(input, 1);
      grantor = pattern.GetMatch(input, 3);
      wxString privs = pattern.GetMatch(input, 2);
      for (unsigned i = 0; i < privs.length(); i++) {
        Privilege priv;
        priv.specifier = privs[i];
        if (i < (privs.length()-1) && privs[i+1] == '*') {
          priv.grantOption = true;
          ++i;
        }
        else {
          priv.grantOption = false;
        }
        privileges.push_back(priv);
      }
    }

    wxString grantee;
    struct Privilege {
      wxChar specifier;
      bool grantOption;
    };
    std::vector<Privilege> privileges;
    wxString grantor;

    template <class OutputIterator>
    void GenerateGrantStatement(OutputIterator output, const wxString& entity, const std::map<wxChar, wxString> &privilegeNames, bool withGrantOption) const
    {
      wxString sql = _T("GRANT ");
      bool first = true;
      for (std::vector<Privilege>::const_iterator iter = privileges.begin(); iter != privileges.end(); iter++) {
        if ((*iter).grantOption != withGrantOption)
          continue;
        std::map<wxChar, wxString>::const_iterator privPtr = privilegeNames.find((*iter).specifier);
        wxASSERT(privPtr != privilegeNames.end());
        if (first)
          first = false;
        else
          sql << _T(", ");
        sql << privPtr->second;
      }
      if (first) return;
      sql << _T(" ON ") << entity << _T(" TO ");
      if (grantee.empty())
        sql << _T("PUBLIC");
      else
        sql << grantee;
      if (withGrantOption)
        sql << _T(" WITH GRANT OPTION");
      *output++ = sql;
    }

  private:
    static wxRegEx pattern;
  };

  std::vector<AclEntry> entries;
};

class PgSettings {
public:
  PgSettings(const wxString &input)
  {
    ScriptWork::ParseTextArray(std::back_inserter(settings), input);
  }

  template <class OutputIterator>
  void GenerateSetStatements(OutputIterator output, ObjectBrowserWork *work, const wxString &prefix = wxEmptyString, bool brackets = false) const
  {
    for (std::vector<Setting>::const_iterator iter = settings.begin(); iter != settings.end(); iter++) {
      (*iter).GenerateSetStatement(output, work, prefix, brackets);
    }
  }

private:
  class Setting {
  public:
    Setting(const wxString &input)
    {
      unsigned splitpt = input.find(_T('='));
      name = input.Left(splitpt);
      value = input.Mid(splitpt + 1);
    }

    template <class OutputIterator>
    void GenerateSetStatement(OutputIterator output, ObjectBrowserWork *work, const wxString &prefix = wxEmptyString, bool brackets = false) const
    {
      wxString sql = prefix;
      sql << _T("SET ");
      if (brackets)
        sql << _T("( ");
      sql << name << _T(" = ");
      if (name == _T("search_path"))
        sql << value;
      else
        sql << work->QuoteLiteral(value);
      if (brackets)
        sql << _T(" )");
      *output++ = sql;
    }

  private:
    wxString name;
    wxString value;
  };

  std::vector<Setting> settings;
};

wxRegEx PgAcl::AclEntry::pattern(_T("^(.*)=([a-zA-Z*]*)/(.+)"));

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
    for (QueryResults::const_iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
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

      if (n != (columns.size()-1)) sql << _T(',');
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

    for (QueryResults::const_iterator iter = columns.begin(); iter != columns.end(); iter++) {
      wxString description = (*iter).ReadText(10);
      if (!description.empty()) {
        *output++ = _T("COMMENT ON COLUMN ") + tableName + _T('.') + (*iter).ReadText(0) + _T(" IS ") + QuoteLiteral(description);
      }
    }

    if (foreignKeys.size() > 0) {
      wxString lastKeyName;
      wxString srcColumns;
      wxString dstColumns;
      const QueryResults::Row *lastKey = NULL;
      for (QueryResults::const_iterator iter = foreignKeys.begin(); iter != foreignKeys.end(); iter++) {
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
    for (QueryResults::const_iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name((*iter).ReadText(0));
      sql << QuoteIdent(name);
      if (n != (columns.size()-1))
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
    for (QueryResults::const_iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name((*iter).ReadText(0));
      sql << _T("            ") << QuoteIdent(name);
      if (n != (columns.size()-1))
        sql << _T(",\n");
      else
        sql << _T("\n");
    }
    sql << _T(") VALUES (\n");
    n = 0;
    for (QueryResults::const_iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name((*iter).ReadText(0)), type((*iter).ReadText(1));
      sql << _T("            <") << QuoteIdent(name) << _T(", ") << type << _T(">");
      if (n != (columns.size()-1))
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
    for (QueryResults::const_iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name((*iter).ReadText(0)), type((*iter).ReadText(1));
      sql << QuoteIdent(name) << _T(" = ")
          << _T("<") << QuoteIdent(name) << _T(", ") << type << _T(">");
      if (n != (columns.size()-1))
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

void SchemaScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row schemaDetail = Query(_T("Schema Detail")).OidParam(nspoid).UniqueResult();
  wxString schemaName = QuoteIdent(schemaDetail[0]);
  switch (mode) {
  case Create: {
    wxString sql;
    sql << _T("CREATE SCHEMA ") << schemaName << _T(" AUTHORIZATION ") << QuoteIdent(schemaDetail[1]);
    *output++ = sql;

    PgAcl(schemaDetail[2]).GenerateGrantStatements(output, schemaDetail[1], schemaName, privilegeMap);
  }
    break;

  case Drop: {
    *output++ = _T("DROP SCHEMA IF EXISTS ") + schemaName;
  }
    break;

  default:
    wxASSERT(false);
  }
}

std::map<wxChar, wxString> SchemaScriptWork::privilegeMap = PrivilegeMap(_T("C=CREATE U=USAGE"));

void ViewScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row viewDetail = Query(_T("View Detail")).OidParam(reloid).UniqueResult();
  QueryResults columns = Query(_T("Relation Column Detail")).OidParam(reloid).List();
  wxString viewName = QuoteIdent(viewDetail[1]) + _T('.') + QuoteIdent(viewDetail[0]);

  switch (mode) {
  case Create:
  case Alter: {
    wxString definition = viewDetail.ReadText(3);

    wxString sql;
    if (mode == Create)
      sql << _T("CREATE VIEW ");
    else
      sql << _T("CREATE OR REPLACE VIEW ");
    sql << viewName << _T(" AS\n") << definition;
    *output++ = sql;

    PgAcl(viewDetail[4]).GenerateGrantStatements(output, viewDetail[2], viewName, privilegeMap);

    AddDescription(output, _T("VIEW"), viewName, viewDetail[5]);
  }
    break;

  case Select: {
    wxString sql;
    sql << _T("SELECT ");
    unsigned n = 0;
    for (QueryResults::const_iterator iter = columns.begin(); iter != columns.end(); iter++, n++) {
      wxString name((*iter).ReadText(0));
      sql << QuoteIdent(name);
      if (n != (columns.size()-1))
        sql << _T(",\n       ");
      else
        sql << _T("\n");
    }
    sql << _T("FROM ") << viewName;
    *output++ = sql;
  }
    break;

  case Drop: {
    wxString sql;
    sql << _T("DROP VIEW IF EXISTS ") << viewName;

    *output++ = sql;
  }
    break;

  default:
    wxASSERT(false);
  }
}

std::map<wxChar, wxString> ViewScriptWork::privilegeMap = PrivilegeMap(_T("r=SELECT"));

void SequenceScriptWork::GenerateScript(OutputIterator output)
{
  wxString sqlTemplate = wxString(owner->GetSql(_T("Sequence Detail")), wxConvUTF8);
  wxString sequenceName = (owner->Query(_T("Sequence Name")).OidParam(reloid).UniqueResult())[0];
  sqlTemplate.Replace(_T("$sequence"), sequenceName, true);
  QueryResults::Row sequenceDetail = owner->DatabaseWork::Query(sqlTemplate.utf8_str()).OidParam(reloid).UniqueResult();

  switch (mode) {
  case Create: {
    wxString sql;
    sql << _T("CREATE SEQUENCE ") << sequenceName;
    sql << _T("\n\tINCREMENT BY ") << sequenceDetail[2];
    sql << _T("\n\tMINVALUE ") << sequenceDetail[3];
    sql << _T("\n\tMAXVALUE ") << sequenceDetail[4];
    sql << _T("\n\tSTART WITH ") << sequenceDetail[5];
    sql << _T("\n\tCACHE ") << sequenceDetail[6];
    bool cycled = sequenceDetail.ReadBool(7);
    sql << ( cycled ? _T("\n\tNO CYCLE") : _T("\n\tCYCLE") );
    sql << _T("\n\tOWNED BY ") << (sequenceDetail[8].empty() ? _T("NONE") : sequenceDetail[8]);

    *output++ = sql;

    *output++ = _T("ALTER SEQUENCE ") + sequenceName + _T(" OWNER TO ") + sequenceDetail[0];

    PgAcl(sequenceDetail[1]).GenerateGrantStatements(output, sequenceDetail[0], _T("SEQUENCE ") + sequenceName, privilegeMap);

    AddDescription(output, _T("SEQUENCE"), sequenceName, sequenceDetail[9]);
  }
    break;

  case Alter: {
    wxString sql;

    sql << _T("ALTER SEQUENCE ") << sequenceName;
    sql << _T("\n\tINCREMENT BY ") << sequenceDetail[2];
    sql << _T("\n\tMINVALUE ") << sequenceDetail[3];
    sql << _T("\n\tMAXVALUE ") << sequenceDetail[4];
    sql << _T("\n\tRESTART WITH ") << sequenceDetail[5];
    sql << _T("\n\tCACHE ") << sequenceDetail[6];
    bool cycled = sequenceDetail.ReadBool(7);
    sql << ( cycled ? _T("\n\tNO CYCLE") : _T("\n\tCYCLE") );
    sql << _T("\n\tOWNED BY ") << (sequenceDetail[8].empty() ? _T("NONE") : sequenceDetail[8]);

    *output++ = sql;

    *output++ = _T("ALTER SEQUENCE ") + sequenceName + _T(" OWNER TO ") + sequenceDetail[0];

    PgAcl(sequenceDetail[1]).GenerateGrantStatements(output, sequenceDetail[0], _T("SEQUENCE ") + sequenceName, privilegeMap);

    AddDescription(output, _T("SEQUENCE"), sequenceName, sequenceDetail[9]);
  }
    break;

  case Drop:
    *output++ = _T("DROP SEQUENCE ") + sequenceName;
    break;

  default:
    wxASSERT(false);
  }
}

std::map<wxChar, wxString> SequenceScriptWork::privilegeMap = PrivilegeMap(_T("r=SELECT w=UPDATE U=USAGE"));

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
