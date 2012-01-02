// -*- mode: c++ -*-

#ifndef __script_events_h
#define __script_events_h

#include "database_event_type.h"
#include "query_results.h"

BEGIN_DECLARE_EVENT_TYPES()
// generated by object browser to create a new editor with script text
  DECLARE_EVENT_TYPE(PQWX_ScriptToWindow, -1)
// generated by editors to indicate they are selected, and reissued every time the database or title changes
  DECLARE_EVENT_TYPE(PQWX_ScriptSelected, -1)
// send by frame to editor from menu bar
  DECLARE_EVENT_TYPE(PQWX_ScriptExecute, -1)
  DECLARE_EVENT_TYPE(PQWX_ScriptDisconnect, -1)
  DECLARE_EVENT_TYPE(PQWX_ScriptReconnect, -1)
// generated by editors at start/stop of execution
  DECLARE_EVENT_TYPE(PQWX_ScriptExecutionBeginning, -1)
  DECLARE_EVENT_TYPE(PQWX_ScriptExecutionFinishing, -1)
// generated by editors as the status of a connection changes
  DECLARE_EVENT_TYPE(PQWX_ScriptConnectionStatus, -1)
// sent asynchronously by execution work when it completes
  DECLARE_EVENT_TYPE(PQWX_ScriptQueryComplete, -1)
END_DECLARE_EVENT_TYPES()

#define PQWX_SCRIPT_TO_WINDOW(id, fn) EVT_DATABASE(id, PQWX_ScriptToWindow, fn)
#define PQWX_SCRIPT_SELECTED(id, fn) EVT_DATABASE(id, PQWX_ScriptSelected, fn)
#define PQWX_SCRIPT_EXECUTE(id, fn) EVT_COMMAND(id, PQWX_ScriptExecute, fn)
#define PQWX_SCRIPT_DISCONNECT(id, fn) EVT_COMMAND(id, PQWX_ScriptDisconnect, fn)
#define PQWX_SCRIPT_RECONNECT(id, fn) EVT_COMMAND(id, PQWX_ScriptReconnect, fn)
#define PQWX_SCRIPT_QUERY_COMPLETE(id, fn) EVT_COMMAND(id, PQWX_ScriptQueryComplete, fn)
#define PQWX_SCRIPT_EXECUTION_BEGINNING(id, fn) EVT_COMMAND(id, PQWX_ScriptExecutionBeginning, fn)
#define PQWX_SCRIPT_EXECUTION_FINISHING(id, fn) EVT_COMMAND(id, PQWX_ScriptExecutionFinishing, fn)
#define PQWX_SCRIPT_CONNECTION_STATUS(id, fn) EVT_COMMAND(id, PQWX_ScriptConnectionStatus, fn)

class PgError;

// The handler of PQWX_ScriptExecutionBeginning sets the event object to one of these to route the results to
class ExecutionResultsHandler {
public:
  virtual void ScriptCommandCompleted(const wxString &statusTag) = 0;
  virtual void ScriptError(const PgError &error) = 0;
  virtual void ScriptResultSet(const wxString &statusTag,
			       const std::vector<ResultField> &fields,
			       const QueryResults &data) = 0;
  virtual void ScriptNotice(const PgError &notice) = 0;
};

#endif
