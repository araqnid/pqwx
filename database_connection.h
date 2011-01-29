// -*- c++ -*-

#ifndef __database_connection_h
#define __database_connection_h

#include "wx/string.h"
#include "wx/thread.h"
#include "server_connection.h"
#include "database_work.h"

class DatabaseWorkerThread;

class DatabaseConnection : public DatabaseQueryExecutor {
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
  void AddWork(DatabaseWork*);
private:
  void setup();
  PGconn *conn;
  ServerConnection *server;
  bool connected;
  bool ExecQuerySync(const char *sql, std::vector< std::vector<wxString> >& results);
  bool ExecCommandSync(const char *sql);
  DatabaseWorkerThread *workerThread;
  wxCriticalSection workerThreadPointer;
  wxMutex workConditionMutex;
  wxCondition workCondition;
  std::vector<DatabaseWork*> initialCommands;
  friend class DatabaseWorkerThread;
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

private:
  std::vector<DatabaseWork*> work;
  bool finish;

protected:
  virtual ExitCode Entry();

private:
  DatabaseConnection *db;

  friend class DatabaseConnection;
};

#endif
