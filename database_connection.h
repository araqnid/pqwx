#ifndef __database_connection_h
#define __database_connection_h

#include "wx/string.h"
#include "server_connection.h"

class DatabaseConnection {
public:
  DatabaseConnection(ServerConnection *server_, PGconn *conn_) {
    server = server_;
    connected = 1;
    conn = conn_;
    setup();
  }

  ~DatabaseConnection() {
    if (connected) dispose();
  }

  void dispose();

  bool ExecQuery(const char *sql, std::vector< std::vector<wxString> >& results);
  bool ExecCommand(const char *sql);

private:
  void setup();
  PGconn *conn;
  ServerConnection *server;
  int connected;
};

#endif
