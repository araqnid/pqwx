#include <deque>
#include "wx/log.h"
#include "server_connection.h"
#include "database_connection.h"
#include "database_work.h"
#include "database_notification_monitor.h"
#include "pqwx.h"

class InitialiseWork : public DatabaseWork {
public:
  void operator()() {
    int clientEncoding = PQclientEncoding(conn);
    const char *clientEncodingName = pg_encoding_to_char(clientEncoding);
    if (strcmp(clientEncodingName, "UTF8") != 0) {
      PQsetClientEncoding(conn, "UTF8");
    }
    DoCommand("SET DateStyle = 'ISO'");
  }
  void NotifyFinished() {
  }
};

class RelabelWork : public DatabaseWork {
public:
  RelabelWork(const wxString &newLabel) : newLabel(newLabel) {}
  void operator()() {
    int serverVersion = PQserverVersion(conn);
    if (serverVersion < 90000)
      return;
    DoCommand(_T("SET application_name = ") + QuoteLiteral(newLabel));
  }
  void NotifyFinished() {
  }
private:
  const wxString newLabel;
};

class DisconnectWork : public DatabaseWork {
public:
  DisconnectWork() {}
  void operator()() {
    PQfinish(conn);
    db->LogDisconnect();
  }
  void NotifyFinished() {
    db->FinishDisconnection();
  }
};

void DatabaseConnection::Connect(ConnectionCallback *callback) {
  connectionCallback = callback;

  wxCriticalSectionLocker stateLocker(workerThread.stateCriticalSection);
  wxASSERT(workerThread.state == NOT_CONNECTED);
  workerThread.state = INITIALISING;
  workerThread.Create();
  workerThread.Run();

  wxMutexLocker queueLocker(workQueueMutex);
  workQueue.push_back(new InitialiseWork());
  workCondition.Signal();
  wxLogDebug(_T("thr#%lx: new database connection worker for '%s'"), workerThread.GetId(), dbname.c_str());
}

void DatabaseConnection::CloseSync() {
  if (!disconnectQueued) {
    if (!BeginDisconnection()) {
      wxLogDebug(_T("%s: CloseSync: not connected"), identification.c_str());
      return;
    }
    wxLogDebug(_T("%s: CloseSync: requested disconnect"), identification.c_str());
  }
  else {
    wxLogDebug(_T("%s: CloseSync: disconnect already requested"), identification.c_str());
  }

  WaitUntilClosed();
}

bool DatabaseConnection::WaitUntilClosed() {
  if (GetState() == NOT_CONNECTED) {
    wxLogDebug(_T("%s: WaitUntilClosed: not connected"), identification.c_str());
    return false;
  }

  wxLogDebug(_T("%s: WaitUntilClosed: waiting for worker completion"), identification.c_str());
  workerThread.Wait();
  workerThread.state = NOT_CONNECTED;

  wxLogDebug(_T("%s: WaitUntilClosed: worker completed after waiting"), identification.c_str());

  return true;
}

wxThread::ExitCode DatabaseConnection::WorkerThread::Entry() {
  SetState(DatabaseConnection::CONNECTING);

  if (!Connect()) {
    SetState(DatabaseConnection::DISCONNECTED);
    return 0;
  }

  wxMutexLocker locker(db->workQueueMutex);

  do {
    while (!db->workQueue.empty()) {
      DatabaseWork *work = db->workQueue.front();
      db->workQueue.pop_front();
      db->workQueueMutex.Unlock();
      SetState(DatabaseConnection::EXECUTING);
      work->db = db;
      work->conn = conn;
      try {
        (*work)();
      } catch (std::exception &e) {
        wxLogDebug(_T("Exception thrown by database work: %s"), wxString(e.what(), wxConvUTF8).c_str());
      } catch (...) {
        wxLogDebug(_T("Unrecognisable exception thrown by database work"));
      }
      work->NotifyFinished();
      delete work;
      SetState(DatabaseConnection::IDLE);
      db->workQueueMutex.Lock();

      ConnStatusType connStatus = PQstatus(conn);
      if (connStatus == CONNECTION_BAD) {
	wxLogDebug(_T("thr#%lx [%s] connection invalid, exiting"), wxThread::GetCurrentId(), db->identification.c_str());
	SetState(DatabaseConnection::DISCONNECTED);
	return 0;
      }
    }

    if (disconnect) {
      wxLogDebug(_T("thr#%lx [%s] disconnection completed"), wxThread::GetCurrentId(), db->identification.c_str());
      SetState(DatabaseConnection::DISCONNECTED);
      return 0;
    }

#ifdef PQWX_NOTIFICATION_MONITOR
    if (notificationReceiver != NULL) {
      PGnotify *notification;
      while ((notification = PQnotifies(conn)) != NULL) {
	(*notificationReceiver)(notification);
      }
      ::wxGetApp().GetNotificationMonitor().AddConnection(DatabaseNotificationMonitor::Client(PQsocket(conn), &monitorProcessor));
    }
#endif

    db->workCondition.Wait();

#ifdef PQWX_NOTIFICATION_MONITOR
    if (notificationReceiver != NULL) {
      ::wxGetApp().GetNotificationMonitor().RemoveConnection(PQsocket(conn));
    }
#endif
  } while (true);

  return 0;
}

static const char *options[] = {
  "host",
  "port",
  "user",
  "password",
  "dbname",
#if PG_VERSION_NUM >= 90000
  "application_name",
#endif
  NULL
};

bool DatabaseConnection::WorkerThread::Connect() {
  const char *values[6];
  // buffer these for the duration of this scope
  wxCharBuffer hostname = db->server.hostname.utf8_str();
  wxCharBuffer portstring = wxString::Format(_T("%d"), db->server.port).utf8_str();
  wxCharBuffer username = db->server.username.utf8_str();
  wxCharBuffer password = db->server.password.utf8_str();
#if PG_VERSION_NUM >= 90000
  wxString appName(_T("pqwx"));
  if (!db->label.IsEmpty()) appName << _T(" - ") << db->label;
  wxCharBuffer appNameBuf = appName.utf8_str();
#endif
  wxCharBuffer dbNameBuf = db->dbname.utf8_str();

  values[0] = db->server.hostname.IsEmpty() ? NULL : hostname.data();

  if (db->server.port > 0) {
    values[1] = portstring.data();
  }
  else {
    values[1] = NULL;
  }

  values[2] = db->server.username.IsEmpty() ? NULL : username.data();
  values[3] = db->server.password.IsEmpty() ? NULL : password.data();
  values[4] = dbNameBuf.data();
#if PG_VERSION_NUM >= 90000
  values[5] = appNameBuf.data();
#endif

#ifdef __WXDEBUG__
  wxLogDebug(_T("thr#%lx connecting to server"), GetId());
  for (int i = 0; options[i]; i++) {
    if (values[i])
      wxLogDebug(_T(" %s => %s"), wxString(options[i], wxConvUTF8).c_str(), wxString(values[i], wxConvUTF8).c_str());
  }
#endif

#if PG_VERSION_NUM >= 90000
  conn = PQconnectdbParams(options, values, 0);
#else
  wxString conninfo;
  for (int j = 0; options[j]; j++) {
    if (values[j])
      conninfo << _T(' ') << wxString(options[j], wxConvUTF8) << _T('=') << wxString(values[j], wxConvUTF8);
  }
#ifdef __WXDEBUG__
  wxLogDebug(_T("conninfo: %s"), conninfo.c_str());
#endif
  conn = PQconnectdb(conninfo.utf8_str());
#endif

  ConnStatusType status = PQstatus(conn);

  if (status == CONNECTION_OK) {
    bool usedPassword = PQconnectionUsedPassword(conn);
    db->LogConnect();
    if (db->connectionCallback)
      db->connectionCallback->OnConnection(usedPassword);
    return true;
  }
  else if (PQconnectionNeedsPassword(conn)) {
    db->LogConnectNeedsPassword();
    if (db->connectionCallback)
      db->connectionCallback->OnConnectionNeedsPassword();
    PQfinish(conn);
    return false;
  }
  else {
    const char *errorMessage = PQerrorMessage(conn);
    db->LogConnectFailed(errorMessage);
    if (db->connectionCallback)
      db->connectionCallback->OnConnectionFailed(wxString(errorMessage, wxConvUTF8));
    PQfinish(conn);
    return false;
  }
}

void DatabaseConnection::LogSql(const char *sql) {
  wxLogDebug(_T("thr#%lx [%s] SQL: %s"), wxThread::GetCurrentId(), identification.c_str(), wxString(sql, wxConvUTF8).c_str());
}

void DatabaseConnection::LogConnect() {
  wxLogDebug(_T("thr#%lx [%s] connected"), wxThread::GetCurrentId(), identification.c_str());
}

void DatabaseConnection::LogConnectFailed(const char *msg) {
  wxLogDebug(_T("thr#%lx [%s] connection FAILED: %s"), wxThread::GetCurrentId(), identification.c_str(), wxString(msg, wxConvUTF8).c_str());
}

void DatabaseConnection::LogConnectNeedsPassword() {
  wxLogDebug(_T("thr#%lx [%s] connection needs password"), wxThread::GetCurrentId(), identification.c_str());
}

void DatabaseConnection::LogDisconnect() {
  wxLogDebug(_T("thr#%lx [%s] disconnected"), wxThread::GetCurrentId(), identification.c_str());
}

void DatabaseConnection::LogSqlQueryInvalidStatus(const char *msg, ExecStatusType status) {
  wxLogDebug(_T("thr#%lx [%s] query produced unexpected status: %s | %s"), wxThread::GetCurrentId(), identification.c_str(), wxString(PQresStatus(status), wxConvUTF8).c_str(), wxString(msg, wxConvUTF8).c_str());
}

void DatabaseConnection::LogSqlQueryFailed(const PgError &error) {
  wxLogDebug(_T("thr#%lx [%s] query failed: %s | %s"), wxThread::GetCurrentId(), identification.c_str(), error.GetSeverity().c_str(), error.GetPrimary().c_str());
}

void DatabaseConnection::AddWork(DatabaseWork *work) {
  wxCriticalSectionLocker stateLocker(workerThread.stateCriticalSection);
  wxCHECK(workerThread.state != NOT_CONNECTED && workerThread.state != DISCONNECTED, );
  workQueue.push_back(work);
  workCondition.Signal();
}

bool DatabaseConnection::AddWorkOnlyIfConnected(DatabaseWork *work) {
  wxCriticalSectionLocker stateLocker(workerThread.stateCriticalSection);
  if (workerThread.state == DISCONNECTED || workerThread.state == NOT_CONNECTED)
    return false;
  wxMutexLocker workQueueLocker(workQueueMutex);
  workQueue.push_back(work);
  workCondition.Signal();
  return true;
}

bool DatabaseConnection::BeginDisconnection() {
  wxCriticalSectionLocker stateLocker(workerThread.stateCriticalSection);
  if (workerThread.state == DISCONNECTED || workerThread.state == NOT_CONNECTED)
    return false;
  wxMutexLocker workQueueLocker(workQueueMutex);
  workQueue.push_back(new DisconnectWork());
  workCondition.Signal();
  disconnectQueued = true;
  return true;
}

void DatabaseConnection::FinishDisconnection() {
  wxCriticalSectionLocker stateLocker(workerThread.stateCriticalSection);
  if (workerThread.state == DISCONNECTED || workerThread.state == NOT_CONNECTED)
    return;
  wxMutexLocker workQueueLocker(workQueueMutex);
  workerThread.disconnect = true;
  workCondition.Signal();
}

void DatabaseConnection::Relabel(const wxString &newLabel) {
  AddWork(new RelabelWork(_T("pqwx - ") + newLabel));
}

void DatabaseConnection::Dispose() {
  State state = GetState();

  if (state == NOT_CONNECTED) {
    wxLogDebug(_T("%s: Dispose: not connected"), identification.c_str());
    return;
  }

  if (state == DISCONNECTED) {
    wxLogDebug(_T("%s: Dispose: disconnected, joining worker thread"), identification.c_str());
    workerThread.Wait();
    workerThread.state = NOT_CONNECTED;
    return;
  }

  wxLogDebug(_T("%s: Dispose: currently connected, doing synchronous close"), identification.c_str());
  CloseSync();
}

#ifdef PQWX_NOTIFICATION_MONITOR

class RegisterWithMonitor : public DatabaseWork {
public:
  RegisterWithMonitor(DatabaseConnection *db, DatabaseConnection::NotificationReceiver *receiver) : db(db), receiver(receiver) {}
private:
  DatabaseConnection *db;
  DatabaseConnection::NotificationReceiver *receiver;
  void operator()()
  {
    db->workerThread.notificationReceiver = receiver;
  }
  void NotifyFinished()
  {
  }
};

class UnregisterWithMonitor : public DatabaseWork {
public:
  UnregisterWithMonitor(DatabaseConnection *db) : db(db) {}
private:
  DatabaseConnection *db;
  void operator()()
  {
    db->workerThread.notificationReceiver = NULL;
  }
  void NotifyFinished()
  {
  }
};

void DatabaseConnection::SetNotificationReceiver(NotificationReceiver *receiver)
{
  if (receiver) {
    AddWorkOnlyIfConnected(new RegisterWithMonitor(this, receiver));
  }
  else {
    AddWorkOnlyIfConnected(new UnregisterWithMonitor(this));
  }
}
#else
void DatabaseConnection::SetNotificationReceiver(NotificationReceiver *receiver)
{
}
#endif
