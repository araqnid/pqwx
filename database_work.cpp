#include <sys/time.h>
#include "database_work.h"
#include "database_connection.h"

using namespace std;

static bool IsSimpleSymbol(const char *str) {
  for (const char *p = str; *p != '\0'; p++) {
    if (!( *p >= 'a' && *p <= 'z' || *p == '_' || *p >= '0' && *p <= '9' ))
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
  for (int i = 0; i < str.length(); i++) {
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
  for (int i = 0; i < str.length(); i++) {
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

bool DatabaseWork::DoCommand(const char *sql) {
  db->LogSql(sql);

  PGresult *rs = PQexec(conn, sql);
  wxASSERT(rs != NULL);

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_COMMAND_OK) {
    db->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
    return false;
  }

  PQclear(rs);

  return true;
}

bool DatabaseWork::DoQuery(const char *sql, QueryResults &results) {
  db->LogSql(sql);

#ifdef PQWX_DEBUG
  struct timeval start;
  gettimeofday(&start, NULL);
#endif

  PGresult *rs = PQexecParams(conn, sql, 0, NULL, NULL, NULL, NULL, 0);
  if (!rs)
    return false;

#ifdef PQWX_DEBUG
  struct timeval finish;
  gettimeofday(&finish, NULL);
  struct timeval elapsed;
  timersub(&finish, &start, &elapsed);
  double elapsedFP = (double) elapsed.tv_sec + ((double) elapsed.tv_usec / 1000000.0);
  wxLogDebug(_T("(%.4lf seconds)"), elapsedFP);
#endif

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_TUPLES_OK) {
#ifdef PQWX_DEBUG
    db->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
    return false; // expected data back
  }

  ReadResultSet(rs, results);

  PQclear(rs);

  return true;
}

bool DatabaseWork::DoQuery(const char *sql, QueryResults &results, Oid paramType, const char *paramValue) {
  db->LogSql(sql);

#ifdef PQWX_DEBUG
  struct timeval start;
  gettimeofday(&start, NULL);
#endif

  PGresult *rs = PQexecParams(conn, sql, 1, &paramType, &paramValue, NULL, NULL, 0);
  if (!rs)
    return false;

#ifdef PQWX_DEBUG
  struct timeval finish;
  gettimeofday(&finish, NULL);
  struct timeval elapsed;
  timersub(&finish, &start, &elapsed);
  double elapsedFP = (double) elapsed.tv_sec + ((double) elapsed.tv_usec / 1000000.0);
  wxLogDebug(_T("(%.4lf seconds)"), elapsedFP);
#endif

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_TUPLES_OK) {
#ifdef PQWX_DEBUG
    db->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
    return false; // expected data back
  }

  ReadResultSet(rs, results);

  PQclear(rs);

  return true;
}

bool DatabaseWork::DoQuery(const char *sql, QueryResults &results, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value) {
  db->LogSql(sql);

#ifdef PQWX_DEBUG
  struct timeval start;
  gettimeofday(&start, NULL);
#endif

  Oid paramTypes[2];
  paramTypes[0] = param1Type;
  paramTypes[1] = param2Type;
  const char *paramValues[2];
  paramValues[0] = param1Value;
  paramValues[1] = param2Value;

  PGresult *rs = PQexecParams(conn, sql, 2, paramTypes, paramValues, NULL, NULL, 0);
  if (!rs)
    return false;

#ifdef PQWX_DEBUG
  struct timeval finish;
  gettimeofday(&finish, NULL);
  struct timeval elapsed;
  timersub(&finish, &start, &elapsed);
  double elapsedFP = (double) elapsed.tv_sec + ((double) elapsed.tv_usec / 1000000.0);
  wxLogDebug(_T("(%.4lf seconds)"), elapsedFP);
#endif

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_TUPLES_OK) {
#ifdef PQWX_DEBUG
    db->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
    return false; // expected data back
  }

  ReadResultSet(rs, results);

  PQclear(rs);

  return true;
}


bool DatabaseWork::DoQuery(const char *sql, QueryResults &results, Oid param1Type, Oid param2Type, Oid param3Type, const char *param1Value, const char *param2Value, const char *param3Value) {
  db->LogSql(sql);

#ifdef PQWX_DEBUG
  struct timeval start;
  gettimeofday(&start, NULL);
#endif

  Oid paramTypes[3];
  paramTypes[0] = param1Type;
  paramTypes[1] = param2Type;
  paramTypes[2] = param3Type;
  const char *paramValues[3];
  paramValues[0] = param1Value;
  paramValues[1] = param2Value;
  paramValues[2] = param3Value;

  PGresult *rs = PQexecParams(conn, sql, 3, paramTypes, paramValues, NULL, NULL, 0);
  if (!rs)
    return false;

#ifdef PQWX_DEBUG
  struct timeval finish;
  gettimeofday(&finish, NULL);
  struct timeval elapsed;
  timersub(&finish, &start, &elapsed);
  double elapsedFP = (double) elapsed.tv_sec + ((double) elapsed.tv_usec / 1000000.0);
  wxLogDebug(_T("(%.4lf seconds)"), elapsedFP);
#endif

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_TUPLES_OK) {
#ifdef PQWX_DEBUG
    db->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
    return false; // expected data back
  }

  ReadResultSet(rs, results);

  PQclear(rs);

  return true;
}

void DatabaseWork::ReadResultSet(PGresult *rs, QueryResults &results) {
  int rowCount = PQntuples(rs);
  int colCount = PQnfields(rs);
  results.reserve(rowCount);
  for (int rowNum = 0; rowNum < rowCount; rowNum++) {
    vector<wxString> row;
    row.reserve(colCount);
    for (int colNum = 0; colNum < colCount; colNum++) {
      const char *value = PQgetvalue(rs, rowNum, colNum);
      row.push_back(wxString(value, wxConvUTF8));
    }
    results.push_back(row);
  }
}
