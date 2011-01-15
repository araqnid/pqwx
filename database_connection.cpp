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

int DatabaseConnection::listDatabases(vector<DatabaseInfo>& databaseList) {
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

int DatabaseConnection::listRoles(vector<RoleInfo>& roleList) {
  PGresult *rs;
  ExecStatusType status;

  if ((rs = PQexec(conn, "SELECT oid, rolname, rolcanlogin, rolsuper FROM pg_roles")) == NULL)
    return 0;
  status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) return 0;
  if (status != PGRES_TUPLES_OK) return 0; // expected data back

  int roleCount = PQntuples(rs);
  for (int rownum = 0; rownum < roleCount; rownum++) {
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

int DatabaseConnection::listTablespaces(vector<TablespaceInfo>& tablespaceList) {
  PGresult *rs;
  ExecStatusType status;

  if ((rs = PQexec(conn, "SELECT oid, spcname FROM pg_tablespace")) == NULL)
    return 0;
  status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) return 0;
  if (status != PGRES_TUPLES_OK) return 0; // expected data back

  int tablespaceCount = PQntuples(rs);
  for (int rownum = 0; rownum < tablespaceCount; rownum++) {
    TablespaceInfo tablespace;

    tablespace.oid = atoi(PQgetvalue(rs, rownum, 0));
    tablespace.name = PQgetvalue(rs, rownum, 1);

    tablespaceList.push_back(tablespace);
  }
  PQclear(rs);

  return 1;
}

int DatabaseConnection::listRelations(vector<RelationInfo> &relationList) {
  PGresult *rs;
  ExecStatusType status;

  if ((rs = PQexec(conn, "SELECT pg_class.oid, nspname, relname, relkind FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace WHERE relkind IN ('r','v','S')")) == NULL)
    return 0;
  status = PQresultStatus(rs);
  if (status == PGRES_FATAL_ERROR) return 0;
  if (status != PGRES_TUPLES_OK) return 0; // expected data back

  int relationCount = PQntuples(rs);
  for (int rownum = 0; rownum < relationCount; rownum++) {
    RelationInfo relation;

    relation.oid = atoi(PQgetvalue(rs, rownum, 0));
    relation.schema = PQgetvalue(rs, rownum, 1);
    relation.name = PQgetvalue(rs, rownum, 2);

    const char *relkind = PQgetvalue(rs, rownum, 3);
    if (strcmp(relkind, "r") == 0) {
      relation.type = RelationInfo::TABLE;
    }
    else if (strcmp(relkind, "v") == 0) {
      relation.type = RelationInfo::VIEW;
    }
    else if (strcmp(relkind, "S") == 0) {
      relation.type = RelationInfo::SEQUENCE;
    }

    relationList.push_back(relation);
  }
  PQclear(rs);

  return 1;
}
