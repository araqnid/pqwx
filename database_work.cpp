#include <stdlib.h>
#include <string.h>
#include "database_work.h"

using namespace std;

void DatabaseBatchWork::addQuery(const char *sql) {
  QueryResults results;
  queryResults.push_back(results);
  work.push_back(new DatabaseQueryWork(sql, &results));
}

void DatabaseBatchWork::addCommand(const char *sql) {
  work.push_back(new DatabaseCommandWork(sql));
}

DatabaseBatchWork::~DatabaseBatchWork() {
  for (std::vector<DatabaseWork*>::iterator iter = work.begin(); iter != work.end(); iter++) {
    delete (*iter);
  }
}

bool DatabaseBatchWork::execute(DatabaseQueryExecutor *db) {
  db->ExecCommandSync("BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY");
  for (std::vector<DatabaseWork*>::iterator iter = work.begin(); iter != work.end(); iter++) {
    (*iter)->execute(db);
  }
  db->ExecCommandSync("END");
}
