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
  connected = 1;
  AddWork(new DatabaseCommandWork("SET client_encoding TO 'UTF8'"));
  AddWork(new DatabaseCommandWork("SET DateStyle = 'ISO'"));
}

void DatabaseConnection::dispose() {
  if (!connected)
    return;

  connected = 0;

  wxCriticalSectionLocker enter(executing);
  PQfinish(conn);
}

bool DatabaseConnection::ExecQuery(const char *sql, vector< vector<wxString> >& results) {
  if (!connected) {
    return ExecQuerySync(sql, results);
  }

  DatabaseWork *work = new DatabaseQueryWork(sql, &results);
  AddWork(work);
  work->await();
  return work->getResult();
}

bool DatabaseConnection::ExecCommand(const char *sql) {
  if (!connected) {
    return ExecCommandSync(sql);
  }

  DatabaseWork *work = new DatabaseCommandWork(sql);
  AddWork(work);
  work->await();
  return work->getResult();
}

void DatabaseConnection::ExecQueryAsync(const char *sql, vector< vector<wxString> >& results, DatabaseWorkCompletionPort *completionPort) {
  AddWork(new DatabaseQueryWork(sql, &results, completionPort));
}

void DatabaseConnection::ExecCommandAsync(const char *sql, DatabaseWorkCompletionPort *completionPort) {
  AddWork(new DatabaseCommandWork(sql, completionPort));
}

void DatabaseConnection::ExecCommandsAsync(vector<const char *> sql, DatabaseWorkCompletionPort *completionPort) {
  int countdown = sql.size();
  for (vector<const char *>::iterator iter = sql.begin(); iter != sql.end(); iter++) {
    if (--countdown == 0) {
      AddWork(new DatabaseCommandWork(*iter, completionPort));
    }
    else {
      AddWork(new DatabaseCommandWork(*iter));
    }
  }
}

bool DatabaseConnection::ExecQuerySync(const char *sql, vector< vector<wxString> >& results) {
  wxCriticalSectionLocker locker(executing);

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
  wxCriticalSectionLocker locker(executing);

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
  db->workConditionMutex.Lock();

  do {
    if (finish) break;
    for (vector<DatabaseWork*>::iterator iter = work.begin(); iter != work.end(); iter++) {
      DatabaseWork *work = *iter;
      bool result = work->execute(db);
      work->finished(result);
    }
    work.clear();
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
