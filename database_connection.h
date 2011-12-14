// -*- c++ -*-

#ifndef __database_connection_h
#define __database_connection_h

#include "wx/string.h"
#include "wx/thread.h"
#include "wx/log.h"
#include "server_connection.h"
#include "database_work.h"

class DatabaseWorkerThread;

class DatabaseConnection {
public:
  DatabaseConnection(ServerConnection *server, const char *dbname, PGconn *conn) : server(server), dbname(dbname), conn(conn), workCondition(workConditionMutex) {
    connected = 0;
    setup();
  }

  ~DatabaseConnection() {
    if (connected) dispose();
  }

  void dispose();

  bool isConnected() { return connected; }
  void AddWork(DatabaseWork*);
  void LogSql(const char *sql);
private:
  void setup();
  char identification[400];
  PGconn *conn;
  ServerConnection *server;
  const char *dbname;
  bool connected;
  DatabaseWorkerThread *workerThread;
  wxCriticalSection workerThreadPointer;
  wxMutex workConditionMutex;
  wxCondition workCondition;
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
