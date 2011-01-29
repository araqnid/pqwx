#include <stdlib.h>
#include <string.h>
#include "server_connection.h"
#include "database_connection.h"

using namespace std;

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

  PQfinish(conn);

  for (vector<DatabaseWork*>::iterator iter = initialCommands.begin(); iter != initialCommands.end(); iter++) {
    delete(*iter);
  }
}

bool DatabaseConnection::ExecQuery(const char *sql, QueryResults& results) {
  if (!connected) {
    return ExecQuerySync(sql, results);
  }

  DatabaseQueryWork work(sql, &results);
  AddWork(&work);
  work.await();
  return work.getResult();
}

bool DatabaseConnection::ExecCommand(const char *sql) {
  if (!connected) {
    return ExecCommandSync(sql);
  }

  DatabaseCommandWork work(sql);
  AddWork(&work);
  work.await();
  return work.getResult();
}

bool DatabaseConnection::ExecQuerySync(const char *sql, QueryResults& results) {
  PGresult *rs = PQexec(conn, sql);
  if (!rs)
    return false;

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_TUPLES_OK)
    return false; // expected data back

  int rowCount = PQntuples(rs);
  int colCount = PQnfields(rs);
  for (int rowNum = 0; rowNum < rowCount; rowNum++) {
    vector<wxString> row;
    for (int colNum = 0; colNum < colCount; colNum++) {
      const char *value = PQgetvalue(rs, rowNum, colNum);
      row.push_back(wxString(value, wxConvUTF8));
    }
    results.push_back(row);
  }

  PQclear(rs);

  return true;
}

bool DatabaseConnection::ExecCommandSync(const char *sql) {
  PGresult *rs = PQexec(conn, sql);
  if (!rs)
    return false;

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_COMMAND_OK)
    return false; // expected acknowledgement

  PQclear(rs);

  return true;
}

wxThread::ExitCode DatabaseWorkerThread::Entry() {
  wxMutexLocker locker(db->workConditionMutex);

  do {
    if (finish) break;
    if (work.size() > 0) {
      vector<DatabaseWork*> workRun(work);
      work.clear();
      db->workConditionMutex.Unlock();
      for (vector<DatabaseWork*>::iterator iter = workRun.begin(); iter != workRun.end(); iter++) {
	DatabaseWork *work = *iter;
	bool result = work->execute(db);
	work->finished(result);
      }
      db->workConditionMutex.Lock();
      continue;
    }
    db->workCondition.Wait();
  } while (true);
}

void DatabaseConnection::AddWork(DatabaseWork *work) {
  wxMutexLocker locker(workConditionMutex);
  if (work)
    workerThread->work.push_back(work);
  else
    workerThread->finish = true;
  workCondition.Signal();
}
