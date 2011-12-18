// -*- c++ -*-

#ifndef __database_work_h
#define __database_work_h

#include <vector>
#include <libpq-fe.h>
#include "wx/string.h"
#include "wx/thread.h"
#include "sql_logger.h"

class DatabaseWork {
public:
  DatabaseWork() {}
  virtual ~DatabaseWork() {}

  virtual void Execute(PGconn *conn) = 0;
  virtual void NotifyFinished() = 0;

  static wxString QuoteIdent(PGconn *conn, const wxString &str) {
    wxCharBuffer buf(str.utf8_str());
    char *escaped = PQescapeIdentifier(conn, buf.data(), strlen(buf.data()));
    wxString result = wxString::FromUTF8(escaped);
    if (result.length() == (str.length() + 2)
	&& result.IsSameAs(_T("\"") + str + _T("\"")))
      return str; 
    PQfreemem(escaped);
    return result;
  }

  static wxString QuoteLiteral(PGconn *conn, const wxString &str) {
    wxCharBuffer buf(str.utf8_str());
    char *escaped = PQescapeLiteral(conn, buf.data(), strlen(buf.data()));
    wxString result = wxString::FromUTF8(escaped);
    PQfreemem(escaped);
    return result;
  }
protected:

  bool DoCommand(PGconn *conn, const wxString &sql) {
    return DoCommand(conn, sql.utf8_str());
  }

  bool DoCommand(PGconn *conn, const char *sql) {
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

  SqlLogger *logger;

  friend class DatabaseWorkerThread;
};

class DisconnectWork : public DatabaseWork {
public:
  class CloseCallback {
  public:
    virtual void OnConnectionClosed() = 0;
  };

  DisconnectWork(CloseCallback *callback = NULL) : callback(callback) {}
  void Execute(PGconn *conn) {
    PQfinish(conn);
    logger->LogDisconnect();
  }
private:
  CloseCallback *callback;
  void NotifyFinished() {
    if (callback)
      callback->OnConnectionClosed();
  }
};

class SnapshotIsolatedWork : virtual public DatabaseWork {
public:
  void Execute(PGconn *conn) {
    if (PQserverVersion(conn) >= 80000) {
      if (!DoCommand(conn, "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY"))
	return;
    }
    else {
      if (!DoCommand(conn, "BEGIN"))
	return;
      if (!DoCommand(conn, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE"))
	return;
      if (!DoCommand(conn, "SET TRANSACTION READ ONLY"))
	return;
    }

    ExecuteInTransaction(conn);

    DoCommand(conn, "END");
  }
  virtual void ExecuteInTransaction(PGconn *conn) = 0;
};

#endif
