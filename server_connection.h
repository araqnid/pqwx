// -*- c++ -*-

#ifndef __db__connection_manager_h
#define __db__connection_manager_h

#include <vector>
#include <string>
#include <map>

#include "database_connection.h"

class ServerConnection {
public:
  ServerConnection() : globalDbName(_T("postgres")) {
    hostname = NULL;
    username = NULL;
    port = -1;
    password = NULL;
  }

  void CloseAllSync() {
    wxLogDebug(_T("%p: Closing all database connections for this server"), this);
    for (std::map<std::string,DatabaseConnection*>::iterator iter = databaseConnections.begin(); iter != databaseConnections.end(); iter++) {
      iter->second->CloseSync();
    }
    databaseConnections.clear();
  }

  // connection parameters
  const char *hostname;
  int port;
  const char *username;
  const char *password;
  const wxString globalDbName;

  DatabaseConnection *getConnection(const wxString& dbname) {
    std::string key(dbname.utf8_str());
    DatabaseConnection *conn = databaseConnections[key];

    if (conn) {
      return conn;
    }

    wxLogDebug(_T("%p: Opening new connection for database '%s'"), this, dbname.c_str());

    conn = new DatabaseConnection(this, dbname);
    databaseConnections[key] = conn;

    return conn;
  }

  DatabaseConnection *getConnection() {
    return getConnection(globalDbName);
  }

private:
  std::map<std::string, DatabaseConnection*> databaseConnections;
  DatabaseConnection *makeConnection(const char *dbname);
};

#endif
