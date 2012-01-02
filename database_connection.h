// -*- c++ -*-

#ifndef __database_connection_h
#define __database_connection_h

#include <set>
#include <deque>
#include "libpq-fe.h"
#include "wx/string.h"
#include "wx/thread.h"
#include "wx/log.h"
#include "server_connection.h"
#include "pg_error.h"

class DatabaseWork;
class DisconnectWork;

class ConnectionCallback {
public:
  virtual void OnConnection(bool usedPassword) = 0;
  virtual void OnConnectionFailed(const wxString &message) = 0;
  virtual void OnConnectionNeedsPassword() = 0;
};

class DatabaseConnection {
public:
  DatabaseConnection(const ServerConnection &server, const wxString &dbname
#if PG_VERSION_NUM >= 90000
		     , const wxString &label = wxEmptyString
#endif
		     )
  : server(server), dbname(dbname), workCondition(workQueueMutex),
#if PG_VERSION_NUM >= 90000
    label(label),
#endif
    workerThread(this), connectionCallback(NULL), disconnectQueued(false)
  {
    identification = server.Identification() + _T(" ") + dbname;
  }

  ~DatabaseConnection() {
    wxCHECK2_MSG(GetState() == NOT_CONNECTED, Dispose(), identification.c_str());
  }

  void Connect(ConnectionCallback *callback = NULL);
  void CloseSync();
  bool WaitUntilClosed();
  void Dispose();
  void AddWork(DatabaseWork*); // will throw an assertion failure if database connection is not live
  bool AddWorkOnlyIfConnected(DatabaseWork *work); // returns true if work added, false if database connection not live
  bool BeginDisconnection(); // returns true if work added, false if already disconnected
  void LogSql(const char *sql);
  void LogDisconnect();
  void LogConnect();
  void LogConnectFailed(const char *msg);
  void LogConnectNeedsPassword();
  void LogSqlQueryInvalidStatus(const char *msg, ExecStatusType status);
  void LogSqlQueryFailed(const PgError &error);
  bool IsConnected() const { State state = workerThread.GetState(); return state != NOT_CONNECTED && state != DISCONNECTED; };
  bool IsAcceptingWork() const { return IsConnected() && !disconnectQueued; }
  const wxString& DbName() const { return dbname; }
  void Relabel(const wxString &newLabel);
  // NOT_CONNECTED - worker thread not started or already joined
  // DISCONNECTED - worker thread finished running, but not joined
  // all others - worker thread is running
  enum State { NOT_CONNECTED, INITIALISING, CONNECTING, IDLE, EXECUTING, DISCONNECTED };
  State GetState() const { return workerThread.GetState(); }
  const wxString& Identification() const { return identification; }
  bool IsStatementPrepared(const wxString& name) const { return preparedStatements.count(name); }
  void MarkStatementPrepared(const wxString& name) { preparedStatements.insert(name); }
private:
  class WorkerThread : public wxThread {
  public:
    WorkerThread(DatabaseConnection *db) : wxThread(wxTHREAD_JOINABLE), db(db), disconnect(false), state(NOT_CONNECTED) { }
  protected:
    virtual ExitCode Entry();
  private:
    PGconn *conn;
    DatabaseConnection *db;
    bool disconnect;
    mutable wxCriticalSection stateCriticalSection;
    State state;

    bool Connect();

    State GetState() const {
      wxCriticalSectionLocker locker(stateCriticalSection);
      return state;
    }

    void SetState(State state_) {
      wxCriticalSectionLocker locker(stateCriticalSection);
      state = state_;
    }

    friend class DatabaseConnection;
  };

  void FinishDisconnection();
  wxString identification;
  ServerConnection server;
  const wxString dbname;
  std::deque<DatabaseWork*> workQueue;
  wxMutex workQueueMutex;
  wxCondition workCondition;
#if PG_VERSION_NUM >= 90000
  wxString label;
#endif
  WorkerThread workerThread;
  ConnectionCallback *connectionCallback;
  std::set<wxString> preparedStatements;
  bool disconnectQueued;

  friend class WorkerThread;
  friend class DisconnectWork;
  friend class DatabaseWork;
};

#endif
