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

  bool DoCommand(const wxString &sql) { return DoCommand(sql.utf8_str()); }
  bool DoCommand(const char *sql);
  bool DoQuery(const char *sql, QueryResults &rs);
  bool DoQuery(const char *sql, QueryResults &rs, Oid paramType, const char *paramValue);
  bool DoQuery(const char *sql, QueryResults &rs, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value);
  bool DoQuery(const char *sql, QueryResults &rs, Oid param1Type, Oid param2Type, Oid param3Type, const char *param1Value, const char *param2Value, const char *param3Value);

  bool DoNamedCommand(const wxString &name) { return DoCommand(sqlDictionary->GetSql(name, PQserverVersion(conn))); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs) { return DoQuery(sqlDictionary->GetSql(name, PQserverVersion(conn)), rs); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs, Oid paramType, const char *paramValue) { return DoQuery(sqlDictionary->GetSql(name, PQserverVersion(conn)), rs, paramType, paramValue); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value) { return DoQuery(sqlDictionary->GetSql(name, PQserverVersion(conn)), rs, param1Type, param2Type, param1Value, param2Value); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs, Oid param1Type, Oid param2Type, Oid param3Type, const char *param1Value, const char *param2Value, const char *param3Value) { return DoQuery(sqlDictionary->GetSql(name, PQserverVersion(conn)), rs, param1Type, param2Type, param3Type, param1Value, param2Value, param3Value); }

  DatabaseConnection *db;
  PGconn *conn;
  const VersionedSql *sqlDictionary;

private:
  void ReadResultSet(PGresult *rs, QueryResults &results);

  friend class DatabaseWorkerThread;
};

#endif
