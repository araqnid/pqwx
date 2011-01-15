// -*- c++ -*-

#ifndef __db__connection_manager_h
#define __db__connection_manager_h

#include "libpq-fe.h"

#include <vector>
#include <string>

class DatabaseInfo {
public:
  int oid;
  std::string name;
  int isTemplate : 1;
  int allowConnections : 1;
  int havePrivsToConnect : 1;
};

class RoleInfo {
public:
  int oid;
  std::string name;
  int canLogin : 1;
  int isSuperuser : 1;
};

class TablespaceInfo {
public:
  int oid;
  std::string name;
};

class ServerConnection {
public:
  ServerConnection() {
    connected = 0;
    hostname = NULL;
    username = NULL;
    port = -1;
    password = NULL;
  }

  ~ServerConnection() {
    if (connected) dispose();
  }

  int connect();
  void dispose();

  // connection parameters
  const char *hostname;
  int port;
  const char *username;
  const char *password;
  int listDatabases(std::vector<DatabaseInfo>&);
  int listRoles(std::vector<RoleInfo>&);
  int listTablespaces(std::vector<TablespaceInfo>&);

private:
  int connected;
  PGconn *conn;
  const char *globalDbName;
  const char *pgVersion;

  int initialise();
};

#endif
