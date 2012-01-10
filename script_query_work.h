/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __script_query_work_h
#define __script_query_work_h

#include <memory>
#include "pg_error.h"
#include "database_work.h"
#include "execution_lexer.h"
#include "script_events.h"

class ScriptExecutionWork : public DatabaseWork {
public:
  /**
   * Create work object
   */
  ScriptExecutionWork(wxEvtHandler *dest, const ExecutionLexer::Token &token) : dest(dest), token(token) {}

  /**
   * Execution result.
   */
  class Result {
  public:
    /**
     * Create result referring back to SQL used for the query.
     */
    Result(const ExecutionLexer::Token& token) : token(token) {}

    /**
     * Read result status.
     */
    void ReadStatus(DatabaseConnection *db, PGconn *conn, PGresult *rs);

    /**
     * Get next state from connection.
     */
    void Finalise(long elapsed_, PGconn *conn) {
      elapsed = elapsed_;
      complete = true;
      if (newConnectionState == Idle) {
	newConnectionState = Decode(PQtransactionStatus(conn));
      }
    }

    /**
     * The offset within the script that caused this execution unit.
     */
    unsigned GetScriptPosition() const { return token.offset; }
  private:
    const ExecutionLexer::Token token;
    ExecStatusType status;
    bool complete;
    std::auto_ptr<QueryResults> data;
    long elapsed;
    PgError error;
    wxString statusTag;
    unsigned long tuplesProcessedCount;
    Oid oidValue;
    bool tuplesProcessedCountValid;
    DatabaseConnectionState newConnectionState;
    friend class ScriptEditorPane; // so, effectively all public...
    friend class ScriptExecutionWork;
    friend class ScriptQueryWork;
    friend class ScriptPutCopyDataWork;
  };

  void NotifyFinished()
  {
    wxCommandEvent event(PQWX_ScriptQueryComplete);
    event.SetClientData(output);
    dest->AddPendingEvent(event);
  }

private:
  wxEvtHandler *dest;

protected:
  const ExecutionLexer::Token token;
  Result *output;

  static DatabaseConnectionState Decode(PGTransactionStatusType txStatus) {
    if (txStatus == PQTRANS_IDLE)
      return Idle;
    else if (txStatus == PQTRANS_INTRANS)
      return IdleInTransaction;
    else if (txStatus == PQTRANS_INERROR)
      return TransactionAborted;
    wxString msg = wxString::Format(_T("Invalid txStatus: %d"), txStatus);
    wxFAIL_MSG(msg);
    abort();
  }
};

/**
 * Execute a query from a script on the database.
 */
class ScriptQueryWork : public ScriptExecutionWork {
public:
  /**
   * Create work object
   */
  ScriptQueryWork(wxEvtHandler *dest, const ExecutionLexer::Token &token, const char *sql) : ScriptExecutionWork(dest, token), sql(sql) {}

  void operator()();
private:
  const char *sql;
};

class ScriptPutCopyDataWork : public ScriptExecutionWork {
public:
  ScriptPutCopyDataWork(wxEvtHandler *dest, const ExecutionLexer::Token &token, const char *buffer) : ScriptExecutionWork(dest, token), buffer(buffer) {}

  void operator()();
private:
  const char *buffer;
};

#endif

// Local Variables:
// mode: c++
// End:
