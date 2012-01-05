// -*- mode: c++ -*-

#ifndef __script_query_work_h
#define __script_query_work_h

#include <memory>
#include "pg_error.h"

class ScriptQueryWork : public DatabaseWork {
public:
  class Result {
  public:
    Result(const ExecutionLexer::Token& token) : token(token) {}
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
    friend class ScriptQueryWork;
  };

  ScriptQueryWork(wxEvtHandler *dest, const ExecutionLexer::Token &token, const char *sql) : dest(dest), token(token), sql(sql) {}

  void Execute() {
    output = new Result(token);
    db->LogSql(sql);
    stopwatch.Start();

    PGresult *rs = PQexecParams(conn, sql, 0, NULL, NULL, NULL, NULL, 0);
    free((void*) sql);
    wxASSERT(rs != NULL);

    output->status = PQresultStatus(rs);
    if (output->status == PGRES_TUPLES_OK) {
      output->data = std::auto_ptr<QueryResults>(new QueryResults(rs));
      output->newConnectionState = Decode(PQtransactionStatus(conn));
    }
    else if (output->status == PGRES_FATAL_ERROR) {
      db->LogSqlQueryFailed(PgError(rs));
      output->error = PgError(rs);
      output->newConnectionState = Decode(PQtransactionStatus(conn));
    }
    else if (output->status == PGRES_COMMAND_OK) {
      output->newConnectionState = Decode(PQtransactionStatus(conn));
    }
    else if (output->status == PGRES_COPY_OUT) {
      output->newConnectionState = CopyToClient;
    }
    else if (output->status == PGRES_COPY_IN) {
      output->newConnectionState = CopyToServer;
    }

    ReadStatus(rs);

    PQclear(rs);

    output->complete = true;

    output->elapsed = stopwatch.Time();
  }

  void NotifyFinished() {
    wxCommandEvent event(PQWX_ScriptQueryComplete);
    event.SetClientData(output);
    dest->AddPendingEvent(event);
  }

private:
  wxEvtHandler *dest;
  const ExecutionLexer::Token token;
  const char *sql;
  wxStopWatch stopwatch;
  Result *output;

  void ReadStatus(PGresult *rs)
  {
    output->statusTag = wxString(PQcmdStatus(rs), wxConvUTF8);
    wxString tuplesCount = wxString(PQcmdTuples(rs), wxConvUTF8);
    if (tuplesCount.empty())
      output->tuplesProcessedCountValid = false;
    else {
      output->tuplesProcessedCountValid = tuplesCount.ToULong(&output->tuplesProcessedCount);
    }
    output->oidValue = PQoidValue(rs);
  }

  static DatabaseConnectionState Decode(PGTransactionStatusType txStatus) {
    if (txStatus == PQTRANS_IDLE)
      return Idle;
    else if (txStatus == PQTRANS_INTRANS)
      return IdleInTransaction;
    else if (txStatus == PQTRANS_INERROR)
      return TransactionAborted;
    wxString msg = _T("Invalid txStatus: ") + txStatus;
    wxFAIL_MSG(msg);
    abort();
  }
};

#endif
