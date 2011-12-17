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
  virtual void OnConnection(bool usedPassword) = 0;
  virtual void OnConnectionFailed(const wxString &message) = 0;
  virtual void OnConnectionNeedsPassword() = 0;
};

class DatabaseConnection : public SqlLogger {
public:
  DatabaseConnection(ServerConnection *server, const wxString &dbname, const wxString &label = wxEmptyString) : server(server), dbname(dbname), workCondition(workConditionMutex), label(label) {
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
  wxString DbName() { return dbname; }
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
  ConnectionCallback *connectionCallback;

  bool AddWorkOnlyIfConnected(DatabaseWork*);

  friend class DatabaseWorkerThread;
};

#endif
