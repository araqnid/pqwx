// -*- c++ -*-

#ifndef __database_connection_h
#define __database_connection_h

#include "libpq-fe.h"
#include "wx/string.h"
#include "wx/thread.h"
#include "wx/log.h"
#include "server_connection.h"

class DatabaseWorkerThread;
class ServerConnection;
class DatabaseWork;

class ConnectionCallback {
public:
  virtual void OnConnection(bool usedPassword) = 0;
  virtual void OnConnectionFailed(const wxString &message) = 0;
  virtual void OnConnectionNeedsPassword() = 0;
};

class DatabaseConnection {
public:
#if PG_VERSION_NUM >= 90000
  DatabaseConnection(const ServerConnection *server, const wxString &dbname, const wxString &label = wxEmptyString) : server(server), dbname(dbname), workCondition(workQueueMutex), workerCompleteCondition(workerStateMutex), label(label) {
    workerThread = NULL;
    state = NOT_CONNECTED;
    connectionCallback = NULL;
    Setup();
  }
#else
  DatabaseConnection(const ServerConnection *server, const wxString &dbname) : server(server), dbname(dbname), workCondition(workQueueMutex), workerCompleteCondition(workerStateMutex) {
    workerThread = NULL;
    state = NOT_CONNECTED;
    connectionCallback = NULL;
    Setup();
  }
#endif

  void Connect(ConnectionCallback *callback = NULL);
  void CloseSync();
  bool WaitUntilClosed();
  void AddWork(DatabaseWork*); // will throw an assertion failure if database connection is not live
  bool AddWorkOnlyIfConnected(DatabaseWork *work); // returns true if work added, false if database connection not live
  void LogSql(const char *sql);
  void LogDisconnect();
  void LogConnect();
  void LogConnectFailed(const char *msg);
  void LogConnectNeedsPassword();
  void LogSqlQueryFailed(const char *msg, ExecStatusType status);
  bool IsConnected();
  const wxString& DbName() const { return dbname; }
  void Relabel(const wxString &newLabel);
  enum State { NOT_CONNECTED, INITIALISING, CONNECTING, IDLE, EXECUTING };
  State GetState();
  const wxString& Identification() const { return identification; }
private:
  void Setup();
  wxString identification;
  const ServerConnection *server;
  const wxString dbname;
#if PG_VERSION_NUM >= 90000
  wxString label;
#endif
  DatabaseWorkerThread *workerThread;
  wxMutex workQueueMutex;
  wxCondition workCondition;
  wxMutex workerStateMutex;
  wxCondition workerCompleteCondition;
  ConnectionCallback *connectionCallback;
  State state;

  friend class DatabaseWorkerThread;
};

#endif
