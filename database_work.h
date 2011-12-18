// -*- c++ -*-

#ifndef __database_work_h
#define __database_work_h

#include <vector>
#include <iostream>
#include <libpq-fe.h>
#include "wx/string.h"
#include "wx/thread.h"
#include "sql_logger.h"

class DatabaseWork {
public:
  DatabaseWork() : done(false) {}
  virtual ~DatabaseWork() {}
  virtual void Execute(PGconn *conn) = 0;
  bool IsDone() {
    wxMutexLocker locker(mutex);
    return done;
  }

  // Notify owner that work is finished (if more is required that signalling the condition).
  // Called from the db execution context with the work mutex held.
  virtual void NotifyFinished() {
  }

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
  wxMutex mutex;
  bool done;

  bool cmd(PGconn *conn, const char *sql) {
    logger->LogSql(sql);

    PGresult *rs = PQexec(conn, sql);

    if (!rs) {
      fwprintf(stderr, _T("No result from PQexec!\n"));
      return false;
    }

    ExecStatusType status = PQresultStatus(rs);
    if (status != PGRES_COMMAND_OK) {
      logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
      return false;
    }

    PQclear(rs);

    return true;
  }

  SqlLogger *logger;

private:
  void Finished() {
    wxMutexLocker locker(mutex);
    done = true;
    NotifyFinished();
  }
  friend class DatabaseWorkerThread;
};

class SynchronisableDatabaseWork : public DatabaseWork {
public:
  SynchronisableDatabaseWork() : condition(mutex) {}
  void await() {
    wxMutexLocker locker(mutex);
    do {
      if (done)
	return;
      condition.Wait();
    } while (true);
  }
private:
  wxCondition condition;
protected:
  void NotifyFinished() {
    condition.Signal();
  }
};

class DisconnectWork : public SynchronisableDatabaseWork {
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
    SynchronisableDatabaseWork::NotifyFinished();
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
protected:
  bool DoCommand(PGconn *conn, const char *sql) {
    return cmd(conn, sql);
  }
};

#endif
