#include <stdlib.h>
#include <string.h>
#include "server_connection.h"
#include "database_connection.h"
#include <wx/log.h>

using namespace std;

class DisconnectWork : public DatabaseWork {
public:
  void execute(PGconn *conn) {
    PQfinish(conn);
  }
};

void DatabaseConnection::setup() {
  wxCriticalSectionLocker enter(workerThreadPointer);
  workerThread = new DatabaseWorkerThread(this);
  workerThread->Create();
  workerThread->Run();
  connected = true;

  initialCommands.push_back(new DatabaseCommandWork("SET client_encoding TO 'UTF8'"));
  initialCommands.push_back(new DatabaseCommandWork("SET DateStyle = 'ISO'"));
  for (vector<DatabaseWork*>::iterator iter = initialCommands.begin(); iter != initialCommands.end(); iter++) {
    AddWork(*iter);
  }
}

void DatabaseConnection::dispose() {
  if (!connected)
    return;

  connected = 0;

  DisconnectWork work;
  AddWork(&work);
  work.await();

  for (vector<DatabaseWork*>::iterator iter = initialCommands.begin(); iter != initialCommands.end(); iter++) {
    delete(*iter);
  }
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
	work->execute(db->conn);
	work->finished();
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

void DatabaseConnection::AddWork(DatabaseWork *work) {
  wxMutexLocker locker(workConditionMutex);
  workerThread->work.push_back(work);
  workCondition.Signal();
}
