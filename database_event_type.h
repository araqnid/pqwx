/**
 * @file
 * Defines the "database event" wxEvent type.
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __database_event_type_h
#define __database_event_type_h

#include "server_connection.h"

/**
 * Database connection state that can be indicated in an event.
 */
enum DatabaseConnectionState { Idle, IdleInTransaction, TransactionAborted, CopyToServer, CopyToClient };

/**
 * A "database event" is a notification event with the context of a database connection and state.
 */
class PQWXDatabaseEvent : public wxNotifyEvent {
public:
  /**
   * Create a database event.
   *
   * @param server Server connection details
   * @param dbname Database name (or empty if not application)
   * @param type Event type
   * @param id Event id
   */
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

/**
 * Prototype for handling a database event.
 */
typedef void (wxEvtHandler::*PQWXDatabaseEventFunction)(PQWXDatabaseEvent&);

/**
 * Static event table macro.
 */
#define EVT_DATABASE(id, type, fn)			  \
    DECLARE_EVENT_TABLE_ENTRY( type, id, -1, \
    (wxObjectEventFunction) (wxEventFunction) (PQWXDatabaseEventFunction) (wxNotifyEventFunction) \
    wxStaticCastEvent( PQWXDatabaseEventFunction, & fn ), (wxObject *) NULL ),

#endif

// Local Variables:
// mode: c++
// End:
