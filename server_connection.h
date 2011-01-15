// -*- c++ -*-

#ifndef __db__connection_manager_h
#define __db__connection_manager_h

#include "libpq-fe.h"

#include <vector>
#include <string>
#include <map>

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

class RelationInfo {
public:
  int oid;
  std::string schema;
  std::string name;
  enum { VIEW, TABLE, SEQUENCE } type;
};

class DatabaseConnection;

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

  DatabaseConnection *getConnection(const char *dbname) {
    std::string key(dbname);
    DatabaseConnection *conn = databaseConnections[key];
    if (conn) {
      return conn;
    }
    return makeConnection(dbname);
  }

  DatabaseConnection *getConnection() {
    return getConnection(globalDbName);
  }

private:
  int connected;
  std::map<std::string, DatabaseConnection*> databaseConnections;
  const char *globalDbName;
  const char *pgVersion;
  DatabaseConnection *makeConnection(const char *dbname);
};

#endif
