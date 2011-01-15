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

  int i;
  for (i = 0; i < 2; i++) {
    values[5] = globalDbNames[i];
    conn = PQconnectdbParams(options, values, 0);
    ConnStatusType status = PQstatus(conn);

    if (status == CONNECTION_OK) {
      connected = 1;
      globalDbName = globalDbNames[i];
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

  connected = 0;

  PQfinish(conn);
}

// fetch some capabilities and basic data from the server after connecting the first time
int ServerConnection::initialise() {
  PGresult *rs;
  ExecStatusType status;

  if ((rs = PQexec(conn, "SET client_encoding TO 'UTF8'")) == NULL)
    return 0;
  status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) return 0;
  if (status != PGRES_COMMAND_OK) return 0; // expected ok

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

  return 1;
}

int ServerConnection::listDatabases(vector<DatabaseInfo>& databaseList) {
  PGresult *rs;
  ExecStatusType status;

  if ((rs = PQexec(conn, "SELECT oid, datname, datistemplate, datallowconn, has_database_privilege(oid, 'connect') AS can_connect FROM pg_database")) == NULL)
    return 0;
  status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) return 0;
  if (status != PGRES_TUPLES_OK) return 0; // expected data back

  int databaseCount = PQntuples(rs);
  for (int rownum = 0; rownum < databaseCount; rownum++) {
    DatabaseInfo db;

    db.oid = atoi(PQgetvalue(rs, rownum, 0));
    db.name = PQgetvalue(rs, rownum, 1);
    db.isTemplate = !strcmp(PQgetvalue(rs, rownum, 2), "t");
    db.allowConnections = !strcmp(PQgetvalue(rs, rownum, 3), "t");
    db.havePrivsToConnect = !strcmp(PQgetvalue(rs, rownum, 4), "t");

    databaseList.push_back(db);
  }
  PQclear(rs);

  return 1;
}

int ServerConnection::listRoles(vector<RoleInfo>& roleList) {
  PGresult *rs;
  ExecStatusType status;

  if ((rs = PQexec(conn, "SELECT oid, rolname, rolcanlogin, rolsuper FROM pg_roles")) == NULL)
    return 0;
  status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) return 0;
  if (status != PGRES_TUPLES_OK) return 0; // expected data back

  int databaseCount = PQntuples(rs);
  for (int rownum = 0; rownum < databaseCount; rownum++) {
    RoleInfo role;

    role.oid = atoi(PQgetvalue(rs, rownum, 0));
    role.name = PQgetvalue(rs, rownum, 1);
    role.canLogin = !strcmp(PQgetvalue(rs, rownum, 2), "t");
    role.isSuperuser = !strcmp(PQgetvalue(rs, rownum, 3), "t");

    roleList.push_back(role);
  }
  PQclear(rs);

  return 1;
}

int ServerConnection::listTablespaces(vector<TablespaceInfo>& tablespaceList) {
  PGresult *rs;
  ExecStatusType status;

  if ((rs = PQexec(conn, "SELECT oid, spcname FROM pg_tablespace")) == NULL)
    return 0;
  status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) return 0;
  if (status != PGRES_TUPLES_OK) return 0; // expected data back

  int databaseCount = PQntuples(rs);
  for (int rownum = 0; rownum < databaseCount; rownum++) {
    TablespaceInfo tablespace;

    tablespace.oid = atoi(PQgetvalue(rs, rownum, 0));
    tablespace.name = PQgetvalue(rs, rownum, 1);

    tablespaceList.push_back(tablespace);
  }
  PQclear(rs);

  return 1;
}
