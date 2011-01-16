#include <stdlib.h>
#include <string.h>
#include "server_connection.h"
#include "database_connection.h"

using namespace std;

void DatabaseConnection::dispose() {
  if (!connected)
    return;

  PQfinish(conn);
  connected = 0;
}

int DatabaseConnection::ExecQuery(const char *sql, vector< vector<wxString> >& results) {
  PGresult *rs = PQexec(conn, sql);
  if (!rs)
    return 0;

  ExecStatusType status = PQresultStatus(rs);
  if (status != PGRES_TUPLES_OK)
    return 0; // expected data back

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

  return 1;
}
