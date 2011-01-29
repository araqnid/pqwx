// -*- c++ -*-

#ifndef __database_work_h
#define __database_work_h

#include <vector>
#include "wx/string.h"
#include "wx/thread.h"

typedef std::vector< std::vector<wxString> > QueryResults;

class DatabaseQueryExecutor {
public:
  virtual bool ExecQuerySync(const char *sql, QueryResults& results) = 0;
  virtual bool ExecCommandSync(const char *sql) = 0;
};

class DatabaseWorkCompletionPort {
public:
  virtual void complete(bool result) = 0;
};

class DatabaseWork {
public:
  DatabaseWork(DatabaseWorkCompletionPort *completion = NULL) : condition(mutex), done(false), completion(completion) {}
  virtual bool execute(DatabaseQueryExecutor *db) = 0;
  void await() {
    wxMutexLocker locker(mutex);
    do {
      if (done)
	return;
      condition.Wait();
    } while (true);
  }
  void finished(bool result_) {
    wxMutexLocker locker(mutex);
    done = true;
    result = result_;
    condition.Signal();
    if (completion) completion->complete(result_);
  }
  bool getResult() {
    wxMutexLocker locker(mutex);
    if (!done)
      fprintf(stderr, "requesting work result when it isn't finished!\n");
    return result;
  }
  DatabaseWorkCompletionPort *completion;
private:
  wxMutex mutex;
  wxCondition condition;
  bool done;
  bool result;
};

class DatabaseQueryWork : public DatabaseWork {
public:
  DatabaseQueryWork(const char *sql, QueryResults *results, DatabaseWorkCompletionPort *completion = NULL) : DatabaseWork(completion), sql(sql), results(results) {}
  bool execute(DatabaseQueryExecutor *db) {
    return db->ExecQuerySync(sql, *results);
  }
private:
  QueryResults *results;
  const char *sql;
};

class DatabaseCommandWork : public DatabaseWork {
public:
  DatabaseCommandWork(const char *sql, DatabaseWorkCompletionPort *completion = NULL) : DatabaseWork(completion), sql(sql) {}
  bool execute(DatabaseQueryExecutor *db) {
    return db->ExecCommandSync(sql);
  }
private:
  const char *sql;
};

class DatabaseBatchWork : public DatabaseWork {
public:
  DatabaseBatchWork(DatabaseWorkCompletionPort *completion = NULL) : DatabaseWork(completion) {};
  ~DatabaseBatchWork();
  void addQuery(const char *sql);
  void addCommand(const char *sql);
  bool execute(DatabaseQueryExecutor *db);
  QueryResults *getQueryResults(int n) {
    return queryResults[n];
  }
private:
  std::vector<QueryResults*> queryResults;
  std::vector<DatabaseWork*> work;
};

#endif
