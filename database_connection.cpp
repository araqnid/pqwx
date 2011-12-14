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
    DatabaseCommandWork encoding("SET client_encoding TO 'UTF8'");
    encoding.execute(logger, conn);
    DatabaseCommandWork datestyle("SET DateStyle = 'ISO'");
    datestyle.execute(logger, conn);
  }
};

void DatabaseConnection::setup() {
  identification[0] = '\0';
  if (server->username != NULL) {
    strcat(identification, server->username);
    strcat(identification, "@");
  }
  if (server->hostname != NULL) {
    strcat(identification, server->hostname);
  }
  else {
    strcat(identification, "<local>");
  }
  if (server->port > 0) {
    strcat(identification, ":");
    sprintf(identification + strlen(identification), "%d", server->port);
  }
  strcat(identification, " ");
  strcat(identification, dbname.utf8_str());
}

void DatabaseConnection::Connect() {
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

  values[0] = db->server->hostname;

  if (db->server->port > 0) {
    values[1] = portbuf;
    sprintf(portbuf, "%d", db->server->port);
  }
  else {
    values[1] = NULL;
  }

  values[2] = db->server->username;
  values[3] = db->server->password;
  values[4] = "pqwx";
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
    return true;
  }
  else if (PQconnectionNeedsPassword(conn)) {
    db->LogConnectNeedsPassword();
    PQfinish(conn);
    return false;
  }
  else {
    db->LogConnectFailed(PQerrorMessage(conn));
    PQfinish(conn);
    return false;
  }
}

void DatabaseConnection::LogSql(const char *sql) {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%s] SQL: %s\n"), wxThread::GetCurrentId(), identification, sql);
#endif
}

void DatabaseConnection::LogConnect() {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%s] connected\n"), wxThread::GetCurrentId(), identification);
#endif
}

void DatabaseConnection::LogConnectFailed(const char *msg) {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%s] connection FAILED: %s\n"), wxThread::GetCurrentId(), identification, msg);
#endif
}

void DatabaseConnection::LogConnectNeedsPassword() {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%s] connection needs password\n"), wxThread::GetCurrentId(), identification);
#endif
}

void DatabaseConnection::LogDisconnect() {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%s] disconnected\n"), wxThread::GetCurrentId(), identification);
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
