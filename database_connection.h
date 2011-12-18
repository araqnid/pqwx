// -*- c++ -*-

#ifndef __database_connection_h
#define __database_connection_h

#include "wx/string.h"
#include "wx/thread.h"
#include "wx/log.h"
#include "server_connection.h"
#include "database_work.h"
#include "sql_logger.h"

class DatabaseWorkerThread;
class ServerConnection;

class ConnectionCallback {
public:
  virtual void OnConnection(bool usedPassword) = 0;
  virtual void OnConnectionFailed(const wxString &message) = 0;
  virtual void OnConnectionNeedsPassword() = 0;
};

class DatabaseConnection : public SqlLogger {
public:
  DatabaseConnection(ServerConnection *server, const wxString &dbname, const wxString &label = wxEmptyString) : server(server), dbname(dbname), workCondition(workConditionMutex), workerCompleteCondition(workerCompleteMutex), label(label) {
    workerThread = NULL;
    workerComplete = false;
    connectionCallback = NULL;
    Setup();
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
private:
  void Setup();
  wxString identification;
  ServerConnection *server;
  const wxString dbname;
  wxString label;
  DatabaseWorkerThread *workerThread;
  wxCriticalSection workerThreadPointer;
  wxMutex workConditionMutex;
  wxCondition workCondition;
  wxMutex workerCompleteMutex;
  wxCondition workerCompleteCondition;
  ConnectionCallback *connectionCallback;
  bool workerComplete;

  friend class DatabaseWorkerThread;
};

#endif
