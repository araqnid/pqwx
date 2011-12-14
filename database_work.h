// -*- c++ -*-

#ifndef __database_work_h
#define __database_work_h

#include <vector>
#include "wx/string.h"
#include "wx/thread.h"

class DatabaseWork {
public:
  DatabaseWork() : condition(mutex), done(false) {}
  virtual ~DatabaseWork() {}
  virtual void execute(PGconn *conn) = 0;
  void await() {
    wxMutexLocker locker(mutex);
    do {
      if (done)
	return;
      condition.Wait();
    } while (true);
  }
  void finished() {
    wxMutexLocker locker(mutex);
    done = true;
    condition.Signal();
  }
  bool isDone() {
    wxMutexLocker locker(mutex);
    return done;
  }
private:
  wxMutex mutex;
  wxCondition condition;
  bool done;
};

class DatabaseCommandWork : public DatabaseWork {
public:
  DatabaseCommandWork(const char *command) : command(command), successful(true) {}
private:
  const char *command;
public:
  bool successful;
  void execute(PGconn *conn) {
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

class DisconnectWork : public DatabaseWork {
public:
  void execute(PGconn *conn) {
    PQfinish(conn);
  }
};

#endif
