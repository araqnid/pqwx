#include "wx/stopwatch.h"
#include "database_work.h"
#include "database_connection.h"

static bool IsSimpleSymbol(const char *str) {
  for (const char *p = str; *p != '\0'; p++) {
    if (!( (*p >= 'a' && *p <= 'z') || *p == '_' || (*p >= '0' && *p <= '9') ))
      return false;
  }
  return true;
}

// This tries to duplicate the fancy behaviour of quote_ident,
// which avoids quoting identifiers when not necessary.
// PQescapeIdentifier is "safe" in that it *always* quotes
wxString DatabaseWork::QuoteIdent(const wxString &str) const {
  if (IsSimpleSymbol(str.utf8_str()))
    return str;
#if PG_VERSION_NUM >= 90000
  wxCharBuffer buf(str.utf8_str());
  char *escaped = PQescapeIdentifier(conn, buf.data(), strlen(buf.data()));
  wxString result = wxString::FromUTF8(escaped);
  PQfreemem(escaped);
#else
  wxString result;
  result.Alloc(str.length() + 2);
  result << _T('\"');
  for (unsigned i = 0; i < str.length(); i++) {
    if (str[i] == '\"')
      result << _T("\"\"");
    else
      result << str[i];
  }
  result << _T('\"');
#endif
  return result;
}

#if PG_VERSION_NUM < 90000
static bool standardConformingStrings(PGconn *conn) {
  const char *value = PQparameterStatus(conn, "standard_conforming_strings");
  return strcmp(value, "on") == 0;
}
#endif

wxString DatabaseWork::QuoteLiteral(const wxString &str) const {
#if PG_VERSION_NUM >= 90000
  wxCharBuffer buf(str.utf8_str());
  char *escaped = PQescapeLiteral(conn, buf.data(), strlen(buf.data()));
  wxString result = wxString::FromUTF8(escaped);
  PQfreemem(escaped);
#else
  wxString result;
  bool useEscapeSyntax = !standardConformingStrings(conn);
  bool usedEscapeSyntax = false;
  result.Alloc(str.length() + 2);
  result << _T('\'');
  for (unsigned i = 0; i < str.length(); i++) {
    if (str[i] == '\'')
      result << _T("\'\'");
    else if (useEscapeSyntax && str[i] == '\\') {
      result << _T("\\\\");
      usedEscapeSyntax = true;
    }
    else
      result << str[i];
  }
  result << _T('\'');
  if (usedEscapeSyntax)
    result = _T('E') + result;
#endif
  return result;
}

void DatabaseWork::DoCommand(const char *sql) const {
  db->LogSql(sql);

  PGresult *rs = PQexec(conn, sql);
  if (rs == NULL) {
    ConnStatusType connStatus = PQstatus(conn);
    if (connStatus == CONNECTION_BAD)
      throw PgLostConnection();
    throw PgInvalidQuery(sql, _T("NULL result from PQexec"));
  }

  ExecStatusType status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) {
    db->LogSqlQueryFailed(PgError(rs));
    throw PgQueryFailure(sql, PgError(rs));
  }
  else if (status != PGRES_COMMAND_OK) {
    db->LogSqlQueryInvalidStatus(PQresultErrorMessage(rs), status);
    throw PgInvalidQuery(sql, _T("unexpected status"));
  }

  PQclear(rs);
}
QueryResults DatabaseWork::DoQuery(const char *sql, int paramCount, const Oid *paramTypes, const char **paramValues) const
{
  db->LogSql(sql);

#ifdef __WXDEBUG__
  wxStopWatch stopwatch;
#endif

  PGresult *rs = PQexecParams(conn, sql, paramCount, paramTypes, paramValues, NULL, NULL, 0);
  if (!rs)
    throw PgResourceFailure();

#ifdef __WXDEBUG__
  wxLogDebug(_T("(%.3lf seconds)"), stopwatch.Time() / 1000.0);
#endif

  ExecStatusType status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) {
    db->LogSqlQueryFailed(PgError(rs));
    throw PgQueryFailure(sql, PgError(rs));
  }
  else if (status != PGRES_TUPLES_OK) {
    db->LogSqlQueryInvalidStatus(PQresultErrorMessage(rs), status);
    throw PgInvalidQuery(sql, _T("expected data back"));
  }

  QueryResults results(rs);

  PQclear(rs);

  return results;
}

QueryResults DatabaseWork::DoQuery(const char *sql, const std::vector<Oid>& paramTypes, const std::vector<wxString>& paramValues) const
{
  unsigned paramCount = paramTypes.size();
  std::vector<wxCharBuffer> buffers;
  std::vector<const char*> values;
  for (unsigned i = 0; i < paramCount; i++) {
    buffers.push_back(paramValues[i].utf8_str());
    values.push_back(buffers.back().data());
  }
  return DoQuery(sql, paramCount, &(paramTypes[0]), &(values[0]));
}

QueryResults DatabaseWork::DoNamedQuery(const wxString &name, const char *sql, int paramCount, const Oid *paramTypes, const char **paramValues) const
{
  if (!db->IsStatementPrepared(name)) {
    db->LogSql((wxString(_T("/* prepare */ ")) + wxString(sql, wxConvUTF8)).utf8_str());

    PGresult *prepareResult = PQprepare(conn, name.utf8_str(), sql, paramCount, paramTypes);
    wxCHECK2(prepareResult, throw PgResourceFailure());
    ExecStatusType prepareStatus = PQresultStatus(prepareResult);
    if (prepareStatus == PGRES_FATAL_ERROR) {
      db->LogSqlQueryFailed(PgError(prepareResult));
      throw PgQueryFailure(name, PgError(prepareResult));
    }
    else if (prepareStatus != PGRES_COMMAND_OK) {
      db->LogSqlQueryInvalidStatus(PQresultErrorMessage(prepareResult), prepareStatus);
      throw PgInvalidQuery(name, _T("unexpected status"));
    }

    db->MarkStatementPrepared(name);
  }
#ifdef __WXDEBUG__
  else {
    db->LogSql((wxString(_T("/* execute */ ")) + wxString(sql, wxConvUTF8)).utf8_str());
  }
#endif

#ifdef __WXDEBUG__
  wxStopWatch stopwatch;
#endif

  PGresult *rs = PQexecPrepared(conn, name.utf8_str(), paramCount, paramValues, NULL, NULL, 0);
  wxCHECK2(rs, throw PgResourceFailure());

#ifdef __WXDEBUG__
  wxLogDebug(_T("(%.3lf seconds)"), stopwatch.Time() / 1000.0);
#endif

  ExecStatusType status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) {
    db->LogSqlQueryFailed(PgError(rs));
    throw PgQueryFailure(name, PgError(rs));
  }
  else if (status != PGRES_TUPLES_OK) {
    db->LogSqlQueryInvalidStatus(PQresultErrorMessage(rs), status);
    throw PgInvalidQuery(name, _T("expected data back"));
  }

  QueryResults results(rs);

  PQclear(rs);

  return results;
}

QueryResults DatabaseWork::DoNamedQuery(const wxString &name, const char *sql, const std::vector<Oid>& paramTypes, const std::vector<wxString>& paramValues) const
{
  unsigned paramCount = paramTypes.size();
  std::vector<wxCharBuffer> buffers;
  std::vector<const char*> values;
  for (unsigned i = 0; i < paramCount; i++) {
    buffers.push_back(paramValues[i].utf8_str());
    values.push_back(buffers.back().data());
  }
  return DoNamedQuery(name, sql, paramCount, &(paramTypes[0]), &(values[0]));
}
// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
