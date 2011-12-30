// -*- c++ -*-

#ifndef __database_connection_h
#define __database_connection_h

#include <set>
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
  DatabaseConnection(const ServerConnection *server, const wxString &dbname, const wxString &label = wxEmptyString) : server(server), dbname(dbname), workCondition(workQueueMutex), label(label) {
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
  ~DatabaseConnection() {
    wxCHECK(workerThread == NULL, );
  }

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
  enum State { NOT_CONNECTED, INITIALISING, CONNECTING, IDLE, EXECUTING, DISCONNECTED };
  State GetState();
  const wxString& Identification() const { return identification; }
  bool IsStatementPrepared(const wxString& name) const { return preparedStatements.count(name); }
  void MarkStatementPrepared(const wxString& name) { preparedStatements.insert(name); }
private:
  void Setup();
  void CleanUpWorkerThread();
  wxString identification;
  const ServerConnection *server;
  const wxString dbname;
  DatabaseWorkerThread *workerThread;
  wxMutex workQueueMutex;
  wxCondition workCondition;
  wxMutex workerStateMutex;
#if PG_VERSION_NUM >= 90000
  wxString label;
#endif
  ConnectionCallback *connectionCallback;
  State state;
  std::set<wxString> preparedStatements;

  friend class DatabaseWorkerThread;
};

#endif
