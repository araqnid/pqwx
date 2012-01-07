/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __database_work_h
#define __database_work_h

#include <vector>
#include "libpq-fe.h"
#include "wx/string.h"
#include "wx/thread.h"
#include "query_results.h"
#include "versioned_sql.h"
#include "database_connection.h"

/**
 * Encapsulates a unit of work to be performed on a database connection.
 */
class DatabaseWork {
public:
  DatabaseWork(const VersionedSql *sqlDictionary = NULL) : sqlDictionary(sqlDictionary) {}
  virtual ~DatabaseWork() {}

  virtual void Execute() = 0;
  virtual void NotifyFinished() = 0;

  wxString QuoteIdent(const wxString &str) const;
  wxString QuoteLiteral(const wxString &str) const;

  bool DoCommand(const wxString &sql) { return DoCommand((const char*) sql.utf8_str()); }
  bool DoCommand(const char *sql);
  QueryResults DoQuery(const char *sql, int paramCount, Oid paramTypes[], const char *paramValues[]);
  QueryResults DoQuery(const char *sql) { return DoQuery(sql, 0, NULL, NULL); }
  QueryResults DoQuery(const char *sql, Oid paramType, const char *paramValue) { return DoQuery(sql, 1, &paramType, &paramValue); }
  QueryResults DoQuery(const char *sql, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value) {
    Oid paramTypes[2];
    paramTypes[0] = param1Type;
    paramTypes[1] = param2Type;
    const char *paramValues[2];
    paramValues[0] = param1Value;
    paramValues[1] = param2Value;
    return DoQuery(sql, 2, paramTypes, paramValues);
  }
  QueryResults DoQuery(const char *sql, Oid param1Type, Oid param2Type, Oid param3Type, const char *param1Value, const char *param2Value, const char *param3Value) {
    Oid paramTypes[3];
    paramTypes[0] = param1Type;
    paramTypes[1] = param2Type;
    paramTypes[2] = param3Type;
    const char *paramValues[3];
    paramValues[0] = param1Value;
    paramValues[1] = param2Value;
    paramValues[2] = param3Value;
    return DoQuery(sql, 3, paramTypes, paramValues);
  }

  bool DoNamedCommand(const wxString &name) { return DoCommand(sqlDictionary->GetSql(name, PQserverVersion(conn))); }
  QueryResults DoNamedQuery(const wxString &name, int paramCount, Oid paramTypes[], const char *paramValues[]);
  QueryResults DoNamedQuery(const wxString &name) { return DoNamedQuery(name, 0, NULL, NULL); }
  QueryResults DoNamedQuery(const wxString &name, Oid paramType, const char *paramValue) { return DoNamedQuery(name, 1, &paramType, &paramValue); }
  QueryResults DoNamedQuery(const wxString &name, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value) {
    Oid paramTypes[2];
    paramTypes[0] = param1Type;
    paramTypes[1] = param2Type;
    const char *paramValues[2];
    paramValues[0] = param1Value;
    paramValues[1] = param2Value;
    return DoNamedQuery(name, 2, paramTypes, paramValues);
  }
  QueryResults DoNamedQuery(const wxString &name, Oid param1Type, Oid param2Type, Oid param3Type, const char *param1Value, const char *param2Value, const char *param3Value) {
    Oid paramTypes[3];
    paramTypes[0] = param1Type;
    paramTypes[1] = param2Type;
    paramTypes[2] = param3Type;
    const char *paramValues[3];
    paramValues[0] = param1Value;
    paramValues[1] = param2Value;
    paramValues[2] = param3Value;
    return DoNamedQuery(name, 3, paramTypes, paramValues);
  }

protected:
  DatabaseConnection *db;
  PGconn *conn;
  const VersionedSql *sqlDictionary;

  friend class DatabaseConnection::WorkerThread;
};

#endif

// Local Variables:
// mode: c++
// End:
