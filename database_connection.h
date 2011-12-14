// -*- c++ -*-

#ifndef __database_connection_h
#define __database_connection_h

#include "wx/string.h"
#include "wx/thread.h"
#include "server_connection.h"
#include "database_work.h"

class DatabaseWorkerThread;

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

  bool isConnected() { return connected; }
  void AddWork(DatabaseWork*);
private:
  void setup();
  PGconn *conn;
  ServerConnection *server;
  bool connected;
  DatabaseWorkerThread *workerThread;
  wxCriticalSection workerThreadPointer;
  wxMutex workConditionMutex;
  wxCondition workCondition;
  std::vector<DatabaseWork*> initialCommands;
  friend class DatabaseWorkerThread;
};

class DatabaseWorkerThread : public wxThread {
public:
  DatabaseWorkerThread(DatabaseConnection *db) : db(db), wxThread(wxTHREAD_DETACHED) {}

  ~DatabaseWorkerThread() {
    wxCriticalSectionLocker enter(db->workerThreadPointer);
    db->workerThread = NULL;
  }

private:
  std::vector<DatabaseWork*> work;

protected:
  virtual ExitCode Entry();

private:
  DatabaseConnection *db;

  friend class DatabaseConnection;
};

#endif
