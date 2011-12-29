#include <sys/time.h>
#include "database_work.h"
#include "database_connection.h"

using namespace std;

bool DatabaseWork::DoCommand(const char *sql) {
  logger->LogSql(sql);

  PGresult *rs = PQexec(conn, sql);
  wxASSERT(rs != NULL);

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_COMMAND_OK) {
    logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
    return false;
  }

  PQclear(rs);

  return true;
}

bool DatabaseWork::DoQuery(const char *sql, QueryResults &results) {
  logger->LogSql(sql);

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
    logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
    return false; // expected data back
  }

  ReadResultSet(rs, results);

  PQclear(rs);

  return true;
}

bool DatabaseWork::DoQuery(const char *sql, QueryResults &results, Oid paramType, const char *paramValue) {
  logger->LogSql(sql);

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
    logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
    return false; // expected data back
  }

  ReadResultSet(rs, results);

  PQclear(rs);

  return true;
}

bool DatabaseWork::DoQuery(const char *sql, QueryResults &results, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value) {
  logger->LogSql(sql);

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
    logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
    return false; // expected data back
  }

  ReadResultSet(rs, results);

  PQclear(rs);

  return true;
}


bool DatabaseWork::DoQuery(const char *sql, QueryResults &results, Oid param1Type, Oid param2Type, Oid param3Type, const char *param1Value, const char *param2Value, const char *param3Value) {
  logger->LogSql(sql);

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
    logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
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
