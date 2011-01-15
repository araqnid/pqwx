#include "server_connection.h"
#include "database_connection.h"

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
  values[4] = "pqwx";

  PGconn *conn;
  int i;
  for (i = 0; i < 2; i++) {
    values[5] = globalDbNames[i];
    conn = PQconnectdbParams(options, values, 0);
    ConnStatusType status = PQstatus(conn);

    if (status == CONNECTION_OK) {
      connected = 1;
      globalDbName = globalDbNames[i];
      fprintf(stderr, "Caching connection to initial database %s\n", globalDbName);
      databaseConnections[globalDbName] = new DatabaseConnection(this, globalDbName, conn);
      return 1;
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

  connected = 0;
}

DatabaseConnection *ServerConnection::makeConnection(const char *dbname) {
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
  values[4] = "pqwx";
  values[5] = dbname;

  PGconn *conn = PQconnectdbParams(options, values, 0);
  ConnStatusType status = PQstatus(conn);

  if (status == CONNECTION_OK) {
    return new DatabaseConnection(this, dbname, conn);
  }
  else {
    fprintf(stderr, "Failed to connect to %s: %s\n", values[5], PQerrorMessage(conn));
    PQfinish(conn);
    return NULL;
  }
}
