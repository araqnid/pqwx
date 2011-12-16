// -*- c++ -*-

#ifndef __db__connection_manager_h
#define __db__connection_manager_h

#include <vector>
#include <string>
#include <map>

#include "postgresql/pg_config.h"
#include "database_connection.h"

class ServerConnection {
public:
  ServerConnection() : globalDbName(_T("postgres")) {
    port = -1;
  }

  void CloseAllSync() {
    wxLogDebug(_T("%p: Closing all database connections for this server"), this);
    for (std::map<std::string,DatabaseConnection*>::iterator iter = databaseConnections.begin(); iter != databaseConnections.end(); iter++) {
      iter->second->CloseSync();
      delete iter->second;
    }
    databaseConnections.clear();
  }

  // connection parameters
  wxString hostname;
  int port;
  wxString username;
  wxString password;
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

  void SetServerName(wxString& serverName) {
    int colon = serverName.Find(_T(':'));
    if (colon == wxNOT_FOUND) {
      hostname = serverName;
    }
    else {
      if (colon > 0)
	hostname = serverName.Mid(0, colon);
      unsigned long portUL;
      serverName.Mid(colon+1).ToULong(&portUL);
      if (portUL == DEF_PGPORT)
	port = 0;
      else
	port = portUL;
    }
  }

  const wxString& Identification() {
    if (identification.IsEmpty()) {
      if (!username.IsEmpty()) {
	identification << username << _T('@');
      }
      if (!hostname.IsEmpty()) {
	identification << hostname;
      }
      else {
	identification << _T("[local]");
      }
      if (port > 0) {
	identification << _T(":") << wxString::Format(_T("%d"), port);
      }
      wxLogDebug(_T("Generated server identification: %s"), identification.c_str());
    }
    return identification;
  }

private:
  std::map<std::string, DatabaseConnection*> databaseConnections;
  DatabaseConnection *makeConnection(const char *dbname);
  wxString identification;
};

#endif
