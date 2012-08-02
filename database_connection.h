/**
 * @file
 * Database connection and worker thread declarations.
 * @author Steve Haslam <araqnid@googlemail.com>
 */

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
#include "database_notification_monitor.h"

class DatabaseWork;
class DisconnectWork;

/**
 * Interface for communicating the result of a connection attempt.
 */
class ConnectionCallback {
public:
  virtual void OnConnection(bool usedPassword) = 0;
  virtual void OnConnectionFailed(const PgError& error) = 0;
  virtual void OnConnectionNeedsPassword() = 0;
};

/**
 * Represents a connection to a server/database.
 *
 * There is a one-to-one correspondence between a DatabaseConnection,
 * a database worker thread and a PGconn*.  All interaction with libpq
 * happens in the context of the database worker thread.
 *
 * A database connection maintains a queue of DatabaseWork
 * objects. The worker thread processes this queue and executes the
 * database work in a FIFO manner.
 *
 * As a special case, the database connection can add a work object to
 * the queue that will cause the worker thread to ensure the server
 * connection is closed and then exit. Once this work object has been
 * <b>added</b> (not necessarily executed), the work queue will not
 * accept any more work objects.
 */
class DatabaseConnection {
public:
  /**
   * Create a database connection.
   *
   * The worker thread is not started nor any connection made until the Connect method is called.
   */
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

  /**
   * Begin connection to database.
   *
   * The database worker thread will be launched and the connection
   * initiated. The result of the connection is communicated via the
   * callback object.
   *
   * The callback <b>must</b> be allocated on the heap- it will be
   * deleted after one of its "OnConnection" methods has been called.
   *
   * If no callback is passed, the result is unknown to the
   * caller... this is not really good, as the caller simply has to
   * hope that the connection is made ok. The option to not supply a
   * callback should be removed at some point.
   *
   * @param callback Connection result
   */
  void Connect(ConnectionCallback *callback = NULL);
  /**
   * Close the connection synchronously.
   *
   * This requests the connection be closed, and waits until that is done.
   */
  void CloseSync();
  /**
   * Request disconnection.
   *
   * @return true if the request has been queued, false if the connection is already disconnected
   */
  bool BeginDisconnection();
  /**
   * Wait until the connection has closed.
   *
   * @return true if waiting was done, false if the connection was already closed.
   */
  bool WaitUntilClosed();
  /**
   * Dispose of this database connection
   *
   * This is basically equivalent to <code>CloseSync</code>.
   */
  void Dispose();
  /**
   * Adds work to this connection's work queue.
   *
   * This will throw an assertion failure if the database connection is not live
   */
  void AddWork(DatabaseWork*);
  /**
   * Adds work to this connections' work queue, if possible.
   *
   * @return true if work added, false if database connection not live
   */
  bool AddWorkOnlyIfConnected(DatabaseWork *work);

  /**
   * Logs SQL performed on this connection.
   *
   * This is typically called by database workers.
   */
  void LogSql(const char *sql);
  /**
   * Logs a disconnection event.
   */
  void LogDisconnect();
  /**
   * Logs a connection event.
   */
  void LogConnect();
  /**
   * Logs a connection failure.
   */
  void LogConnectFailed(const PgError& error);
  /**
   * Logs an indication that connection failed due to no password.
   */
  void LogConnectNeedsPassword();
  /**
   * Logs an indication that a query failed due to reaching an invalid state.
   */
  void LogSqlQueryInvalidStatus(const char *msg, ExecStatusType status);
  /**
   * Logs an indication that a query failed due to the server throwing an error.
   */
  void LogSqlQueryFailed(const PgError &error);
  /**
   * Tests if this connection is actually connected.
   */
  bool IsConnected() const { State state = workerThread.GetState(); return state != NOT_CONNECTED && state != DISCONNECTED; };
  /**
   * Tests if this connection is able to accept work.
   *
   * @return false if the connection is disconnected, or a disconnection has been requested
   */
  bool IsAcceptingWork() const { return IsConnected() && !disconnectQueued; }
  /**
   * @return the database name this connection is for
   */
  const wxString& DbName() const { return dbname; }
  /**
   * Change the label (application_name) of this connection.
   */
  void Relabel(const wxString &newLabel);
  /**
   * Connection state.
   */
  enum State { 
    /**
     * worker thread not started or already joined
     */
    NOT_CONNECTED,
    /**
     * worker thread is starting up
     */
    INITIALISING,
    /**
     * connection in progress
     */
    CONNECTING,
    /**
     * worker thread waiting for work
     */
    IDLE,
    /**
     * worker thread is executing work
     */
    EXECUTING,
    /**
     * worker thread finished running, but not joined
     */
    DISCONNECTED };
  /**
   * @return the state of this connection
   */
  State GetState() const { return workerThread.GetState(); }
  /**
   * @return a string identifying this connection
   */
  const wxString& Identification() const { return identification; }

  /**
   * Interface for receiving an asynchronous notification from the monitor.
   */
  class NotificationReceiver {
  public:
    virtual void operator()(PGnotify*) = 0;
  };

  /**
   * Associate this connection with a notification receiver.
   */
  void SetNotificationReceiver(NotificationReceiver *receiver);

  /**
   * @return true if a statement with the given name has been prepared on this connection
   */
  bool IsStatementPrepared(const wxString& name) const { return preparedStatements.count(name); }
  /**
   * Mark a statement as having been prepared on this connection.
   */
  void MarkStatementPrepared(const wxString& name) { preparedStatements.insert(name); }
private:
  class WorkerThread : public wxThread {
  public:
    WorkerThread(DatabaseConnection *db) : wxThread(wxTHREAD_JOINABLE), db(db), disconnect(false), state(NOT_CONNECTED),
                                           notificationReceiver(NULL)
#ifdef PQWX_NOTIFICATION_MONITOR
                                         , monitorProcessor(this)
#endif
  { }
  protected:
    virtual ExitCode Entry();
  private:
    PGconn *conn;
    DatabaseConnection *db;
    bool disconnect;
    mutable wxCriticalSection stateCriticalSection;
    State state;
    NotificationReceiver *notificationReceiver;
#ifdef PQWX_NOTIFICATION_MONITOR
    class MonitorInputProcessor : public DatabaseNotificationMonitor::InputProcessor {
    public:
      MonitorInputProcessor(WorkerThread *worker) : worker(worker) {}
      void operator()()
      {
        PQconsumeInput(worker->conn);
        PGnotify *notification;
        while ((notification = PQnotifies(worker->conn)) != NULL) {
          (*worker->notificationReceiver)(notification);
        }
      }
    private:
      WorkerThread *worker;
    } monitorProcessor;
#endif

    bool Connect();
    void HandleNotification();
    void CheckConnectionStatus();
    void DeleteRemainingWork();

    State GetState() const {
      wxCriticalSectionLocker locker(stateCriticalSection);
      return state;
    }

    void SetState(State state_) {
      wxCriticalSectionLocker locker(stateCriticalSection);
      state = state_;
    }

    friend class DatabaseConnection;
    friend class MonitorInputProcessor;
    friend class RegisterWithMonitor;
    friend class UnregisterWithMonitor;
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
  friend class RegisterWithMonitor;
  friend class UnregisterWithMonitor;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
