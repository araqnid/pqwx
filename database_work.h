// -*- c++ -*-

#ifndef __database_work_h
#define __database_work_h

class DatabaseQueryExecutor {
public:
  virtual bool ExecQuerySync(const char *sql, std::vector< std::vector<wxString> >& results) = 0;
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
  }
  bool getResult() {
    wxMutexLocker locker(mutex);
    if (!done)
      fprintf(stderr, "requesting work result when it isn't finished!\n");
    return result;
  }
private:
  DatabaseWorkCompletionPort *completion;
  wxMutex mutex;
  wxCondition condition;
  DatabaseQueryExecutor *db;
  bool done;
  bool result;
};

class DatabaseQueryWork : public DatabaseWork {
public:
  DatabaseQueryWork(const char *sql, std::vector< std::vector<wxString> > *results, DatabaseWorkCompletionPort *completion = NULL) : DatabaseWork(completion), sql(sql), results(results) {}
  bool execute(DatabaseQueryExecutor *db) {
    return db->ExecQuerySync(sql, *results);
  }
private:
  std::vector< std::vector<wxString> > *results;
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

#endif
