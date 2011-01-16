// -*- c++ -*-

#ifndef __database_connection_h
#define __database_connection_h

#include "wx/string.h"
#include "wx/thread.h"
#include "server_connection.h"

class DatabaseWorkerThread;
class DatabaseWork;

class DatabaseWorkCompletionPort {
public:
  virtual void complete(bool result) = 0;
};

class DatabaseConnection {
public:
  DatabaseConnection(ServerConnection *server_, PGconn *conn_) : workCondition(workConditionMutex) {
    server = server_;
    conn = conn_;
    connected = 0;
    setup();
  }

  ~DatabaseConnection() {
    if (connected) dispose();
  }

  void dispose();

  bool ExecQuery(const char *sql, std::vector< std::vector<wxString> >& results);
  bool ExecCommand(const char *sql);
private:
  void setup();
  PGconn *conn;
  ServerConnection *server;
  int connected;
  void AddWork(DatabaseWork*);
public:
  void ExecQueryAsync(const char *sql, std::vector< std::vector<wxString> >& results, DatabaseWorkCompletionPort *completion);
  void ExecCommandAsync(const char *sql, DatabaseWorkCompletionPort *completion);
  void ExecCommandsAsync(std::vector<const char *> sql, DatabaseWorkCompletionPort *completion);
  bool ExecQuerySync(const char *sql, std::vector< std::vector<wxString> >& results);
  bool ExecCommandSync(const char *sql);
public:
  DatabaseWorkerThread *workerThread;
  wxCriticalSection executing;
  wxCriticalSection workerThreadPointer;
  wxMutex workConditionMutex;
  wxCondition workCondition;
};

class DatabaseWorkerThread : public wxThread {
public:
  DatabaseWorkerThread(DatabaseConnection *db) : db(db), wxThread(wxTHREAD_DETACHED) {
    finish = false;
  }

  ~DatabaseWorkerThread() {
    wxCriticalSectionLocker enter(db->workerThreadPointer);
    db->workerThread = NULL;
  }

  std::vector<DatabaseWork*> work;
  bool finish;

protected:
  virtual ExitCode Entry();

private:
  DatabaseConnection *db;
};

class DatabaseWork {
public:
  DatabaseWork(DatabaseWorkCompletionPort *completion = NULL) : condition(mutex), done(false), completion(completion) {}
  virtual bool execute(DatabaseConnection *db) = 0;
  void await() {
    wxMutexLocker locker(mutex);
    do {
      if (done)
	return;
      condition.Wait();
    } while (true);
  }
  void finished(bool result_) {
    wxMutexLocker locker(mutex);
    done = true;
    result = result_;
    condition.Signal();
  }
  bool getResult() {
    wxMutexLocker locker(mutex);
    if (!done)
      fprintf(stderr, "requesting work result when it isn't finished!\n");
    return result;
  }
private:
  DatabaseWorkCompletionPort *completion;
  wxMutex mutex;
  wxCondition condition;
  DatabaseConnection *db;
  bool done;
  bool result;
};

class DatabaseQueryWork : public DatabaseWork {
public:
  DatabaseQueryWork(const char *sql, std::vector< std::vector<wxString> > *results, DatabaseWorkCompletionPort *completion = NULL) : DatabaseWork(completion), sql(sql), results(results) {}
  bool execute(DatabaseConnection *db) {
    return db->ExecQuerySync(sql, *results);
  }
private:
  std::vector< std::vector<wxString> > *results;
  const char *sql;
};

class DatabaseCommandWork : public DatabaseWork {
public:
  DatabaseCommandWork(const char *sql, DatabaseWorkCompletionPort *completion = NULL) : DatabaseWork(completion), sql(sql) {}
  bool execute(DatabaseConnection *db) {
    return db->ExecCommandSync(sql);
  }
private:
  const char *sql;
};

#endif
