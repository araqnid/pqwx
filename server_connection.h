// -*- c++ -*-

#ifndef __db__connection_manager_h
#define __db__connection_manager_h

#include "libpq-fe.h"

#include <vector>
#include <string>

class ServerConnection {
public:
  ServerConnection(const char *name_);
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
  const char * const name;
  int listDatabases(std::vector<std::string>&);

private:
  int connected;
  PGconn *conn;
  const char *globalDbName;
  const char *pgVersion;

  int initialise();
};

#endif
