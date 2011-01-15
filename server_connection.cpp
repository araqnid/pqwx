#include <stdlib.h>
#include <string.h>
#include "server_connection.h"

using namespace std;

static const char *options[] = {
  "host",
  "port",
  "user",
  "password",
  "application_name",
  "dbname",
  NULL
};

static const char *globalDbNames[] = { "postgres", "template1" };

ServerConnection::ServerConnection(const char *name_) : name(name_) {
  connected = 0;
  hostname = NULL;
  username = NULL;
  port = -1;
  password = NULL;
}

int ServerConnection::connect() {
  const char *values[5];
  char portbuf[10];

  values[0] = hostname;

  if (port > 0) {
    values[1] = portbuf;
    sprintf(portbuf, "%d", port);
  }
  else {
    values[1] = NULL;
  }

  values[2] = username;
  values[3] = password;
  values[4] = name;

  int i;
  for (i = 0; i < 2; i++) {
    values[5] = globalDbNames[i];
    conn = PQconnectdbParams(options, values, 0);
    ConnStatusType status = PQstatus(conn);

    if (status == CONNECTION_OK) {
      connected = 1;
      globalDbName = globalDbNames[i];
      fprintf(stderr, "Connected to %s\n", values[5]);
      if (initialise()) {
	return 1;
      }
      else {
	fprintf(stderr, "Failed to initialise: %s\n", values[5], PQerrorMessage(conn));
	PQfinish(conn);
	return 0;
      }
    }
    else {
      fprintf(stderr, "Failed to connect to %s: %s\n", values[5], PQerrorMessage(conn));
      PQfinish(conn);
    }
  }

  return 0;
}

void ServerConnection::dispose() {
  if (!connected)
    return;

  fprintf(stderr, "Closing %s connection\n", name);

  connected = 0;

  PQfinish(conn);
}

// fetch some capabilities and basic data from the server after connecting the first time
int ServerConnection::initialise() {
  PGresult *rs;
  ExecStatusType status;

  if ((rs = PQexec(conn, "SELECT version()")) == NULL)
    return 0;
  status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) return 0;
  if (status != PGRES_TUPLES_OK) return 0; // expected data back

  char *value = PQgetvalue(rs, 0, 0);
  char *storage = (char*) malloc(strlen(value)+1);
  strcpy(storage, value);
  pgVersion = storage;

  PQclear(rs);

  fprintf(stderr, "Server version: %s\n", pgVersion);

  return 1;
}

int ServerConnection::listDatabases(vector<string>& databaseList) {
  PGresult *rs;
  ExecStatusType status;

  if ((rs = PQexec(conn, "SELECT oid, datname, datistemplate, datallowconn, has_database_privilege(oid, 'connect') AS can_connect FROM pg_database")) == NULL)
    return 0;
  status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) return 0;
  if (status != PGRES_TUPLES_OK) return 0; // expected data back

  int databaseCount = PQntuples(rs);
  for (int rownum = 0; rownum < databaseCount; rownum++) {
    char *oidstr = PQgetvalue(rs, rownum, 0);
    char *datname = PQgetvalue(rs, rownum, 1);
    char *datistemplate = PQgetvalue(rs, rownum, 2);
    char *datallowconn = PQgetvalue(rs, rownum, 3);
    char *canconnect = PQgetvalue(rs, rownum, 4);

    fprintf(stderr, "Found database: %s=%s template? %s can-connect? %s privs-to-connect? %s\n", oidstr, datname, datistemplate, datallowconn, canconnect);
    databaseList.push_back(datname);
  }
  PQclear(rs);

  return 1;
}
