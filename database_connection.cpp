#include <stdlib.h>
#include <string.h>
#include "server_connection.h"
#include "database_connection.h"
#include <wx/log.h>

using namespace std;

class DisconnectWork : public DatabaseWork {
public:
  void execute(SqlLogger *logger, PGconn *conn) {
    PQfinish(conn);
    logger->LogDisconnect();
  }
};

class InitialiseWork : public DatabaseWork {
public:
  void execute(SqlLogger *logger, PGconn *conn) {
    int clientEncoding = PQclientEncoding(conn);
    const char *clientEncodingName = pg_encoding_to_char(clientEncoding);
    if (strcmp(clientEncodingName, "UTF8") != 0) {
      PQsetClientEncoding(conn, "UTF8");
    }
    DatabaseCommandWork datestyle("SET DateStyle = 'ISO'");
    datestyle.execute(logger, conn);
  }
};

class RelabelWork : public DatabaseWork {
public:
  RelabelWork(const wxString &newLabel) : newLabel(newLabel) {}
  void execute(SqlLogger *logger, PGconn *conn) {
    wxString sql = _T("SET application_name = ") + quoteLiteral(conn, newLabel);
    wxCharBuffer sqlBuf = sql.utf8_str();
    DatabaseCommandWork command(sqlBuf.data());
    command.execute(logger, conn);
  }
private:
  const wxString newLabel;
};

class DatabaseWorkerThread : public wxThread {
public:
  DatabaseWorkerThread(DatabaseConnection *db) : db(db), wxThread(wxTHREAD_DETACHED) {
    dbname = strdup(db->dbname.utf8_str());
  }

  ~DatabaseWorkerThread() {
    wxCriticalSectionLocker enter(db->workerThreadPointer);
    db->workerThread = NULL;
    free((void*) dbname);
  }
private:
  std::vector<DatabaseWork*> work;
  PGconn *conn;

protected:
  virtual ExitCode Entry();

private:
  bool Connect();

  DatabaseConnection *db;
  const char *dbname;

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
  workerThread->work.push_back(new InitialiseWork());
  workCondition.Signal();
  wxLogDebug(_T("thr#%lx: new database connection worker for '%s'"), workerThread->GetId(), dbname.c_str());
}

void DatabaseConnection::CloseSync() {
  DisconnectWork work;
  if (AddWorkOnlyIfConnected(&work)) {
    work.await();
  }
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
    if (work.size() > 0) {
      vector<DatabaseWork*> workRun(work);
      work.clear();
      db->workConditionMutex.Unlock();
      for (vector<DatabaseWork*>::iterator iter = workRun.begin(); iter != workRun.end(); iter++) {
	DatabaseWork *work = *iter;
	work->execute(db, conn);
	work->finished(); // after this method is called, don't touch work again.
      }
      db->workConditionMutex.Lock();
      continue;
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
  values[5] = dbname;

#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx connecting to server\n"), GetId());
  for (int i = 0; options[i]; i++) {
    fwprintf(stderr, wxT(" %s => %s\n"), options[i], values[i]);
  }
#endif

  conn = PQconnectdbParams(options, values, 0);
  ConnStatusType status = PQstatus(conn);

  if (status == CONNECTION_OK) {
    db->LogConnect();
    if (db->connectionCallback)
      db->connectionCallback->OnConnection();
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
  workerThread->work.push_back(work);
  workCondition.Signal();
}

bool DatabaseConnection::AddWorkOnlyIfConnected(DatabaseWork *work) {
  wxCriticalSectionLocker enter(workerThreadPointer);
  wxMutexLocker locker(workConditionMutex);
  if (workerThread == NULL)
    return false;
  workerThread->work.push_back(work);
  workCondition.Signal();
  return true;
}

void DatabaseConnection::Relabel(const wxString &newLabel) {
  AddWork(new RelabelWork(_T("pqwx - ") + newLabel));
}
