// -*- c++ -*-

#ifndef __database_work_h
#define __database_work_h

#include <vector>
#include "libpq-fe.h"
#include "wx/string.h"
#include "wx/thread.h"
#include "query_results.h"
#include "versioned_sql.h"
#include "database_connection.h"

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
  bool DoQuery(const char *sql, QueryResults &rs, int paramCount, Oid paramTypes[], const char *paramValues[]);
  bool DoQuery(const char *sql, QueryResults &rs) { return DoQuery(sql, rs, 0, NULL, NULL); }
  bool DoQuery(const char *sql, QueryResults &rs, Oid paramType, const char *paramValue) { return DoQuery(sql, rs, 1, &paramType, &paramValue); }
  bool DoQuery(const char *sql, QueryResults &rs, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value) {
    Oid paramTypes[2];
    paramTypes[0] = param1Type;
    paramTypes[1] = param2Type;
    const char *paramValues[2];
    paramValues[0] = param1Value;
    paramValues[1] = param2Value;
    return DoQuery(sql, rs, 2, paramTypes, paramValues);
  }
  bool DoQuery(const char *sql, QueryResults &rs, Oid param1Type, Oid param2Type, Oid param3Type, const char *param1Value, const char *param2Value, const char *param3Value) {
    Oid paramTypes[3];
    paramTypes[0] = param1Type;
    paramTypes[1] = param2Type;
    paramTypes[2] = param3Type;
    const char *paramValues[3];
    paramValues[0] = param1Value;
    paramValues[1] = param2Value;
    paramValues[2] = param3Value;
    return DoQuery(sql, rs, 3, paramTypes, paramValues);
  }

  bool DoNamedCommand(const wxString &name) { return DoCommand(sqlDictionary->GetSql(name, PQserverVersion(conn))); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs, int paramCount, Oid paramTypes[], const char *paramValues[]);
  bool DoNamedQuery(const wxString &name, QueryResults &rs) { return DoNamedQuery(name, rs, 0, NULL, NULL); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs, Oid paramType, const char *paramValue) { return DoNamedQuery(name, rs, 1, &paramType, &paramValue); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value) {
    Oid paramTypes[2];
    paramTypes[0] = param1Type;
    paramTypes[1] = param2Type;
    const char *paramValues[2];
    paramValues[0] = param1Value;
    paramValues[1] = param2Value;
    return DoNamedQuery(name, rs, 2, paramTypes, paramValues);
  }
  bool DoNamedQuery(const wxString &name, QueryResults &rs, Oid param1Type, Oid param2Type, Oid param3Type, const char *param1Value, const char *param2Value, const char *param3Value) {
    Oid paramTypes[3];
    paramTypes[0] = param1Type;
    paramTypes[1] = param2Type;
    paramTypes[2] = param3Type;
    const char *paramValues[3];
    paramValues[0] = param1Value;
    paramValues[1] = param2Value;
    paramValues[2] = param3Value;
    return DoNamedQuery(name, rs, 3, paramTypes, paramValues);
  }

  DatabaseConnection *db;
  PGconn *conn;
  const VersionedSql *sqlDictionary;

private:
  void ReadResultSet(PGresult *rs, QueryResults &results);

  friend class DatabaseWorkerThread;
};

#endif
