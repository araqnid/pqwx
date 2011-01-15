#ifndef __database_connection_h
#define __database_connection_h

#include "server_connection.h"

class DatabaseConnection {
public:
  DatabaseConnection(ServerConnection *server_, PGconn *conn_) {
    server = server_;
    connected = 1;
    conn = conn_;
  }

  ~DatabaseConnection() {
    if (connected) dispose();
  }

  int listDatabases(std::vector<DatabaseInfo>&);
  int listRoles(std::vector<RoleInfo>&);
  int listTablespaces(std::vector<TablespaceInfo>&);
  int listRelations(std::vector<RelationInfo>&);
  void dispose();

private:
  PGconn *conn;
  ServerConnection *server;
  int connected;
};

#endif
