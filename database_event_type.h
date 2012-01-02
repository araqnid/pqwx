// -*- mode: c++ -*-

#ifndef __database_event_type_h
#define __database_event_type_h

#include "server_connection.h"

// some events indicate a server/database context
// optionally this can include a connection state

enum DatabaseConnectionState { Idle, IdleInTransaction, TransactionAborted, CopyToServer, CopyToClient };

class PQWXDatabaseEvent : public wxNotifyEvent {
public:
  PQWXDatabaseEvent(const ServerConnection &server, const wxString &dbname, wxEventType type = wxEVT_NULL, int id = 0)
    : wxNotifyEvent(type, id), server(server), dbname(dbname), hasState(false) {}

  const ServerConnection& GetServer() const { return server; }
  bool DatabaseSpecified() const { return !dbname.empty(); }
  const wxString& GetDatabase() const { return dbname; }

  void SetConnectionState(DatabaseConnectionState state_) { state = state_; hasState = true; }
  bool HasConnectionState() const { return hasState; }
  DatabaseConnectionState GetConnectionState() const { wxASSERT(hasState); return state; }

private:
  ServerConnection server;
  wxString dbname;
  DatabaseConnectionState state;
  bool hasState;
};

typedef void (wxEvtHandler::*PQWXDatabaseEventFunction)(PQWXDatabaseEvent&);

#define EVT_DATABASE(id, type, fn)			  \
    DECLARE_EVENT_TABLE_ENTRY( type, id, -1, \
    (wxObjectEventFunction) (wxEventFunction) (PQWXDatabaseEventFunction) (wxNotifyEventFunction) \
    wxStaticCastEvent( PQWXDatabaseEventFunction, & fn ), (wxObject *) NULL ),

#endif
