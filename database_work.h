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
  DatabaseWork() : condition(mutex), done(false) {}
  virtual ~DatabaseWork() {}
  virtual void execute(SqlLogger *logger, PGconn *conn) = 0;
  void await() {
    wxMutexLocker locker(mutex);
    do {
      if (done)
	return;
      condition.Wait();
    } while (true);
  }
  bool isDone() {
    wxMutexLocker locker(mutex);
    return done;
  }
  // Notify owner that work is finished (if more is required that signalling the condition).
  // Called from the db execution context with the work mutex held.
  virtual void notifyFinished() {
  }
protected:
private:
  wxMutex mutex;
  wxCondition condition;
  bool done;
  void finished() {
    wxMutexLocker locker(mutex);
    done = true;
    condition.Signal();
    notifyFinished();
  }
  friend class DatabaseWorkerThread;
};

class DatabaseCommandWork : public DatabaseWork {
public:
  DatabaseCommandWork(const char *command) : command(command), successful(true) {}
private:
  const char *command;
public:
  bool successful;
  void execute(SqlLogger *logger, PGconn *conn) {
    logger->LogSql(command);

    PGresult *rs = PQexec(conn, command);
    if (!rs)
      return;

    ExecStatusType status = PQresultStatus(rs);
    if (status != PGRES_COMMAND_OK)
      return;

    PQclear(rs);

    successful = true;
  }
};

#endif
