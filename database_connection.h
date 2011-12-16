// -*- c++ -*-

#ifndef __database_connection_h
#define __database_connection_h

#include "wx/string.h"
#include "wx/thread.h"
#include "wx/log.h"
#include "server_connection.h"
#include "database_work.h"
#include "sql_logger.h"

enum DatabaseConnectionState {
  NOT_CONNECTED,
  CONNECTING,
  CONNECTED,
  IDLE,
  EXECUTING
};

class DatabaseWorkerThread;
class ServerConnection;

class ConnectionCallback {
public:
  virtual void OnConnection() = 0;
  virtual void OnConnectionFailed(const wxString &message) = 0;
  virtual void OnConnectionNeedsPassword() = 0;
};

class DatabaseConnection : public SqlLogger {
public:
  DatabaseConnection(ServerConnection *server, const wxString &dbname) : server(server), dbname(dbname), workCondition(workConditionMutex) {
    workerThread = NULL;
    connectionCallback = NULL;
    Setup();
  }

  void Connect(ConnectionCallback *callback = NULL);
  void CloseSync();
  void AddWork(DatabaseWork*);
  void LogSql(const char *sql);
  void LogDisconnect();
  void LogConnect();
  void LogConnectFailed(const char *msg);
  void LogConnectNeedsPassword();
  void LogSqlQueryFailed(const char *msg, ExecStatusType status);
  DatabaseConnectionState GetState();
  bool IsConnected();
private:
  void Setup();
  wxString identification;
  ServerConnection *server;
  const wxString dbname;
  DatabaseWorkerThread *workerThread;
  wxCriticalSection workerThreadPointer;
  wxMutex workConditionMutex;
  wxCondition workCondition;
  ConnectionCallback *connectionCallback;

  bool AddWorkOnlyIfConnected(DatabaseWork*);

  friend class DatabaseWorkerThread;
};

#endif
