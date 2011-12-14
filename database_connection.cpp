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
  strcat(identification, dbname);

  wxCriticalSectionLocker enter(workerThreadPointer);
  workerThread = new DatabaseWorkerThread(this);
  workerThread->Create();
  workerThread->Run();
  connected = true;

  LogConnect();

  AddWork(new InitialiseWork());
}

void DatabaseConnection::dispose() {
  if (!connected)
    return;

  connected = 0;

  DisconnectWork work;
  AddWork(&work);
  work.await();
}

wxThread::ExitCode DatabaseWorkerThread::Entry() {
  wxMutexLocker locker(db->workConditionMutex);

  do {
    if (work.size() > 0) {
      vector<DatabaseWork*> workRun(work);
      work.clear();
      db->workConditionMutex.Unlock();
      for (vector<DatabaseWork*>::iterator iter = workRun.begin(); iter != workRun.end(); iter++) {
	DatabaseWork *work = *iter;
	work->execute(db, db->conn);
	work->finished(); // after this method is called, don't touch work again.
      }
      db->workConditionMutex.Lock();
      continue;
    }
    ConnStatusType connStatus = PQstatus(db->conn);
    if (connStatus == CONNECTION_BAD) {
      db->connected = false;
      break;
    }
    db->workCondition.Wait();
  } while (true);
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

void DatabaseConnection::LogDisconnect() {
#ifdef PQWX_DEBUG
  fwprintf(stderr, wxT("thr#%lx [%s] disconnected\n"), wxThread::GetCurrentId(), identification);
#endif
}

void DatabaseConnection::AddWork(DatabaseWork *work) {
  wxMutexLocker locker(workConditionMutex);
  workerThread->work.push_back(work);
  workCondition.Signal();
}
