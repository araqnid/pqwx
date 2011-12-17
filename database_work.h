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
  virtual void execute(SqlLogger *logger, PGconn *conn) = 0;
  bool isDone() {
    wxMutexLocker locker(mutex);
    return done;
  }

  // Notify owner that work is finished (if more is required that signalling the condition).
  // Called from the db execution context with the work mutex held.
  virtual void notifyFinished() {
  }

  static wxString quoteIdent(PGconn *conn, const wxString &str) {
    wxCharBuffer buf(str.utf8_str());
    char *escaped = PQescapeIdentifier(conn, buf.data(), strlen(buf.data()));
    wxString result = wxString::FromUTF8(escaped);
    if (result.length() == (str.length() + 2)
	&& result.IsSameAs(_T("\"") + str + _T("\"")))
      return str; 
    PQfreemem(escaped);
    return result;
  }

  static wxString quoteLiteral(PGconn *conn, const wxString &str) {
    wxCharBuffer buf(str.utf8_str());
    char *escaped = PQescapeLiteral(conn, buf.data(), strlen(buf.data()));
    wxString result = wxString::FromUTF8(escaped);
    PQfreemem(escaped);
    return result;
  }
protected:
  wxMutex mutex;
  bool done;
private:
  void finished() {
    wxMutexLocker locker(mutex);
    done = true;
    notifyFinished();
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
  void notifyFinished() {
    condition.Signal();
  }
};

class DatabaseCommandWork : public DatabaseWork {
public:
  DatabaseCommandWork(const char *command) : command(command), successful(true), paramType(0) {}
  DatabaseCommandWork(const char *command, Oid argtype, const char *argvalue) : command(command), successful(true), paramType(paramType), paramValue(paramValue) {}
private:
  const char *command;
  Oid paramType;
  const char *paramValue;
public:
  bool successful;
  void execute(SqlLogger *logger, PGconn *conn) {
    logger->LogSql(command);

    PGresult *rs;
    if (paramType)
      rs = PQexecParams(conn, command, 1, &paramType, &paramValue, NULL, NULL, 0);
    else
      rs = PQexec(conn, command);

    if (!rs) {
      fwprintf(stderr, _T("No result from PQexec!\n"));
      return;
    }

    ExecStatusType status = PQresultStatus(rs);
    if (status != PGRES_COMMAND_OK) {
      logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
      return;
    }

    PQclear(rs);

    successful = true;
  }
};

class DisconnectWork : public SynchronisableDatabaseWork {
public:
  class CloseCallback {
  public:
    virtual void OnConnectionClosed() = 0;
  };

  DisconnectWork(CloseCallback *callback = NULL) : callback(callback) {}
  void execute(SqlLogger *logger, PGconn *conn) {
    PQfinish(conn);
    logger->LogDisconnect();
  }
private:
  CloseCallback *callback;
  void notifyFinished() {
    SynchronisableDatabaseWork::notifyFinished();
    if (callback)
      callback->OnConnectionClosed();
  }
};

#endif
