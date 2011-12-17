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

  // connection parameters
  wxString hostname;
  int port;
  wxString username;
  wxString password;
  const wxString globalDbName;

  // discovered attributes
  bool passwordNeededToConnect;

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
  wxString identification;
};

#endif
