#include <deque>
#include "wx/log.h"
#include "server_connection.h"
#include "database_connection.h"

using namespace std;

class InitialiseWork : public DatabaseWork {
public:
  void Execute(PGconn *conn) {
    int clientEncoding = PQclientEncoding(conn);
    const char *clientEncodingName = pg_encoding_to_char(clientEncoding);
    if (strcmp(clientEncodingName, "UTF8") != 0) {
      PQsetClientEncoding(conn, "UTF8");
    }
    DoCommand(conn, "SET DateStyle = 'ISO'");
  }
  void NotifyFinished() {
  }
};

class RelabelWork : public DatabaseWork {
public:
  RelabelWork(const wxString &newLabel) : newLabel(newLabel) {}
  void Execute(PGconn *conn) {
    int serverVersion = PQserverVersion(conn);
    if (serverVersion < 90000)
      return;
    DoCommand(conn, _T("SET application_name = ") + QuoteLiteral(conn, newLabel));
  }
  void NotifyFinished() {
  }
private:
  const wxString newLabel;
};

class DatabaseWorkerThread : public wxThread {
public:
  DatabaseWorkerThread(DatabaseConnection *db) : db(db), wxThread(wxTHREAD_DETACHED) { }

  ~DatabaseWorkerThread() {
    wxCriticalSectionLocker enter(db->workerThreadPointer);
    db->workerThread = NULL;
  }
private:
  deque<DatabaseWork*> workQueue;
  PGconn *conn;

protected:
  virtual ExitCode Entry();

private:
  bool Connect();

  DatabaseConnection *db;

  friend class DatabaseConnection;
};

void DatabaseConnection::Setup() {
  identification = server->Identification() + _T(" ") + dbname;
}

void DatabaseConnection::Connect(ConnectionCallback *callback) {
  connectionCallback = callback;

  wxCriticalSectionLocker enter(workerThreadPointer);
  wxASSERT(workerThread == NULL);
  workerThread = new DatabaseWorkerThread(this);
  workerThread->Create();
  workerThread->Run();

  wxMutexLocker locker(workConditionMutex);
  workerThread->workQueue.push_back(new InitialiseWork());
  workCondition.Signal();
  wxLogDebug(_T("thr#%lx: new database connection worker for '%s'"), workerThread->GetId(), dbname.c_str());
}

void DatabaseConnection::CloseSync() {
  wxMutexLocker locker(workerCompleteMutex);

  if (!IsConnected()) {
    wxLogDebug(_T("%s: CloseSync: no worker thread"), identification.c_str());
    return;
  }
  if (workerComplete) {
    wxLogDebug(_T("%s: CloseSync: worker thread already complete (maybe exiting?)"), identification.c_str());
    return;
  }

  wxLogDebug(_T("%s: CloseSync: adding disconnect work"), identification.c_str());
  AddWork(new DisconnectWork());

  do {
    wxLogDebug(_T("%s: CloseSync: waiting for worker completion"), identification.c_str());
    workerCompleteCondition.Wait();
  } while (!workerComplete);

  wxLogDebug(_T("%s: CloseSync: worker completed after waiting"), identification.c_str());
}

bool DatabaseConnection::WaitUntilClosed() {
  wxMutexLocker locker(workerCompleteMutex);

  if (!IsConnected()) {
    wxLogDebug(_T("%s: WaitUntilClosed: no worker thread"), identification.c_str());
    return false;
  }
  if (workerComplete) {
    wxLogDebug(_T("%s: WaitUntilClosed: worker thread already complete (maybe exiting?)"), identification.c_str());
    return false;
  }

  do {
    wxLogDebug(_T("%s: WaitUntilClosed: waiting for worker completion"), identification.c_str());
    workerCompleteCondition.Wait();
  } while (!workerComplete);

  wxLogDebug(_T("%s: WaitUntilClosed: worker completed after waiting"), identification.c_str());

  return true;
}

bool DatabaseConnection::IsConnected() {
  wxCriticalSectionLocker enter(workerThreadPointer);
  return workerThread != NULL;
}

wxThread::ExitCode DatabaseWorkerThread::Entry() {
  if (!Connect()) {
    return 0;
  }

  wxMutexLocker locker(db->workConditionMutex);

  do {
    while (!workQueue.empty()) {
      DatabaseWork *work = workQueue.front();
      workQueue.pop_front();
      db->workConditionMutex.Unlock();
      work->logger = db;
      work->Execute(conn);
      work->NotifyFinished();
      delete work;
      db->workConditionMutex.Lock();
    }
    ConnStatusType connStatus = PQstatus(conn);
    if (connStatus == CONNECTION_BAD) {
      break;
    }
    db->workCondition.Wait();
  } while (true);

  wxMutexLocker completionLocker(db->workerCompleteMutex);
  db->workerComplete = true;
  db->workerCompleteCondition.Signal();

  return 0;
}

static const char *options[] = {
  "host",
  "port",
  "user",
  "password",
  "application_name",
  "dbname",
  NULL
};

bool DatabaseWorkerThread::Connect() {
  const char *values[5];
  char portbuf[10];
  // buffer these for the duration of this scope
  wxCharBuffer hostname = db->server->hostname.utf8_str();
  wxCharBuffer username = db->server->username.utf8_str();
  wxCharBuffer password = db->server->password.utf8_str();
  wxString appName(_T("pqwx"));
  if (!db->label.IsEmpty()) appName << _T(" - ") << db->label;
  wxCharBuffer appNameBuf = appName.utf8_str();
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
  values[4] = appNameBuf.data();
  values[5] = dbNameBuf.data();

#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx connecting to server\n"), GetId());
  for (int i = 0; options[i]; i++) {
    fwprintf(stderr, wxT(" %s => %s\n"), options[i], values[i]);
  }
#endif

  conn = PQconnectdbParams(options, values, 0);
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
  fwprintf(stderr, wxT("thr#%lx [%ls] SQL: %s\n"), wxThread::GetCurrentId(), identification.c_str(), sql);
#endif
}

void DatabaseConnection::LogConnect() {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%ls] connected\n"), wxThread::GetCurrentId(), identification.c_str());
#endif
}

void DatabaseConnection::LogConnectFailed(const char *msg) {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%ls] connection FAILED: %s\n"), wxThread::GetCurrentId(), identification.c_str(), msg);
#endif
}

void DatabaseConnection::LogConnectNeedsPassword() {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%ls] connection needs password\n"), wxThread::GetCurrentId(), identification.c_str());
#endif
}

void DatabaseConnection::LogDisconnect() {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%ls] disconnected\n"), wxThread::GetCurrentId(), identification.c_str());
#endif
}

void DatabaseConnection::LogSqlQueryFailed(const char *msg, ExecStatusType status) {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%ls] query failed: %s | %s\n"), wxThread::GetCurrentId(), identification.c_str(), PQresStatus(status), msg);
#endif
}

void DatabaseConnection::AddWork(DatabaseWork *work) {
  wxCriticalSectionLocker enter(workerThreadPointer);
  wxMutexLocker locker(workConditionMutex);
  wxCHECK(workerThread != NULL, );
  workerThread->workQueue.push_back(work);
  workCondition.Signal();
}

bool DatabaseConnection::AddWorkOnlyIfConnected(DatabaseWork *work) {
  wxCriticalSectionLocker enter(workerThreadPointer);
  wxMutexLocker locker(workConditionMutex);
  if (workerThread == NULL)
    return false;
  workerThread->workQueue.push_back(work);
  workCondition.Signal();
  return true;
}

void DatabaseConnection::Relabel(const wxString &newLabel) {
  AddWork(new RelabelWork(_T("pqwx - ") + newLabel));
}
