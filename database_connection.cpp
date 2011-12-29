#include <deque>
#include "wx/log.h"
#include "server_connection.h"
#include "database_connection.h"
#include "database_work.h"
#include "disconnect_work.h"

using namespace std;

class InitialiseWork : public DatabaseWork {
public:
  void Execute() {
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
  void Execute() {
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

class DatabaseWorkerThread : public wxThread {
public:
  DatabaseWorkerThread(DatabaseConnection *db) : wxThread(wxTHREAD_DETACHED), db(db) { }

  ~DatabaseWorkerThread() {
    wxMutexLocker locker(db->workerStateMutex);
    db->workerThread = NULL;
    db->state = DatabaseConnection::NOT_CONNECTED;
    db->workerCompleteCondition.Signal();
  }
private:
  deque<DatabaseWork*> workQueue;
  PGconn *conn;

protected:
  virtual ExitCode Entry();

private:
  bool Connect();

  void SetState(DatabaseConnection::State state) {
    wxMutexLocker locker(db->workerStateMutex);
    db->state = state;
  }

  DatabaseConnection *db;

  friend class DatabaseConnection;
};

void DatabaseConnection::Setup() {
  identification = server->Identification() + _T(" ") + dbname;
}

void DatabaseConnection::Connect(ConnectionCallback *callback) {
  connectionCallback = callback;

  wxMutexLocker stateLocker(workerStateMutex);
  wxASSERT(workerThread == NULL);
  state = INITIALISING;
  workerThread = new DatabaseWorkerThread(this);
  workerThread->Create();
  workerThread->Run();

  wxMutexLocker queueLocker(workQueueMutex);
  workerThread->workQueue.push_back(new InitialiseWork());
  workCondition.Signal();
  wxLogDebug(_T("thr#%lx: new database connection worker for '%s'"), workerThread->GetId(), dbname.c_str());
}

void DatabaseConnection::CloseSync() {
  wxMutexLocker locker(workerStateMutex);

  if (state == NOT_CONNECTED) {
    wxLogDebug(_T("%s: CloseSync: not connected"), identification.c_str());
    return;
  }

  wxLogDebug(_T("%s: CloseSync: adding disconnect work"), identification.c_str());
  // can't call AddWork here because that would try to re-obtain the mutex
  workerThread->workQueue.push_back(new DisconnectWork());
  wxCHECK(workerThread != NULL, );
  workCondition.Signal();

  do {
    wxLogDebug(_T("%s: CloseSync: waiting for worker completion"), identification.c_str());
    workerCompleteCondition.Wait();
  } while (state != NOT_CONNECTED);

  wxLogDebug(_T("%s: CloseSync: worker completed after waiting"), identification.c_str());
}

bool DatabaseConnection::WaitUntilClosed() {
  wxMutexLocker locker(workerStateMutex);

  if (state == NOT_CONNECTED) {
    wxLogDebug(_T("%s: WaitUntilClosed: not connected"), identification.c_str());
    return false;
  }

  do {
    wxLogDebug(_T("%s: WaitUntilClosed: waiting for worker completion"), identification.c_str());
    workerCompleteCondition.Wait();
  } while (state != NOT_CONNECTED);

  wxLogDebug(_T("%s: WaitUntilClosed: worker completed after waiting"), identification.c_str());

  return true;
}

bool DatabaseConnection::IsConnected() {
  wxMutexLocker locker(workerStateMutex);
  return state != NOT_CONNECTED;
}

wxThread::ExitCode DatabaseWorkerThread::Entry() {
  SetState(DatabaseConnection::CONNECTING);

  if (!Connect()) {
    return 0;
  }

  wxMutexLocker locker(db->workQueueMutex);

  do {
    while (!workQueue.empty()) {
      DatabaseWork *work = workQueue.front();
      workQueue.pop_front();
      db->workQueueMutex.Unlock();
      SetState(DatabaseConnection::EXECUTING);
      work->db = db;
      work->conn = conn;
      work->Execute();
      work->NotifyFinished();
      delete work;
      SetState(DatabaseConnection::IDLE);
      db->workQueueMutex.Lock();
    }
    ConnStatusType connStatus = PQstatus(conn);
    if (connStatus == CONNECTION_BAD) {
      break;
    }
    db->workCondition.Wait();
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

bool DatabaseWorkerThread::Connect() {
  const char *values[5];
  char portbuf[10];
  // buffer these for the duration of this scope
  wxCharBuffer hostname = db->server->hostname.utf8_str();
  wxCharBuffer username = db->server->username.utf8_str();
  wxCharBuffer password = db->server->password.utf8_str();
#if PG_VERSION_NUM >= 90000
  wxString appName(_T("pqwx"));
  if (!db->label.IsEmpty()) appName << _T(" - ") << db->label;
  wxCharBuffer appNameBuf = appName.utf8_str();
#endif
  wxCharBuffer dbNameBuf = db->dbname.utf8_str();

  values[0] = db->server->hostname.IsEmpty() ? NULL : hostname.data();

  if (db->server->port > 0) {
    values[1] = portbuf;
    sprintf(portbuf, "%d", db->server->port);
  }
  else {
    values[1] = NULL;
  }

  values[2] = db->server->username.IsEmpty() ? NULL : username.data();
  values[3] = db->server->password.IsEmpty() ? NULL : password.data();
  values[4] = dbNameBuf.data();
#if PG_VERSION_NUM >= 90000
  values[5] = appNameBuf.data();
#endif

#ifdef PQWX_DEBUG
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
#ifdef PQWX_DEBUG
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
#ifdef PQWX_DEBUG
  wxLogDebug(_T("thr#%lx [%s] SQL: %s"), wxThread::GetCurrentId(), identification.c_str(), wxString(sql, wxConvUTF8).c_str());
#endif
}

void DatabaseConnection::LogConnect() {
#ifdef PQWX_DEBUG
  wxLogDebug(_T("thr#%lx [%s] connected"), wxThread::GetCurrentId(), identification.c_str());
#endif
}

void DatabaseConnection::LogConnectFailed(const char *msg) {
#ifdef PQWX_DEBUG
  wxLogDebug(_T("thr#%lx [%s] connection FAILED: %s"), wxThread::GetCurrentId(), identification.c_str(), wxString(msg, wxConvUTF8).c_str());
#endif
}

void DatabaseConnection::LogConnectNeedsPassword() {
#ifdef PQWX_DEBUG
  wxLogDebug(_T("thr#%lx [%s] connection needs password"), wxThread::GetCurrentId(), identification.c_str());
#endif
}

void DatabaseConnection::LogDisconnect() {
#ifdef PQWX_DEBUG
  wxLogDebug(_T("thr#%lx [%s] disconnected"), wxThread::GetCurrentId(), identification.c_str());
#endif
}

void DatabaseConnection::LogSqlQueryFailed(const char *msg, ExecStatusType status) {
#ifdef PQWX_DEBUG
  wxLogDebug(_T("thr#%lx [%s] query failed: %s | %s"), wxThread::GetCurrentId(), identification.c_str(), wxString(PQresStatus(status), wxConvUTF8).c_str(), wxString(msg, wxConvUTF8).c_str());
#endif
}

void DatabaseConnection::AddWork(DatabaseWork *work) {
  wxMutexLocker stateLocker(workerStateMutex);
  wxCHECK(workerThread != NULL, );
  workerThread->workQueue.push_back(work);
  workCondition.Signal();
}

bool DatabaseConnection::AddWorkOnlyIfConnected(DatabaseWork *work) {
  wxMutexLocker stateLocker(workerStateMutex);
  if (workerThread == NULL)
    return false;
  wxMutexLocker workQueueLocker(workQueueMutex);
  workerThread->workQueue.push_back(work);
  workCondition.Signal();
  return true;
}

void DatabaseConnection::Relabel(const wxString &newLabel) {
  AddWork(new RelabelWork(_T("pqwx - ") + newLabel));
}
