// -*- mode: c++ -*-

#ifndef __script_query_work_h
#define __script_query_work_h

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
    QueryResults data;
    std::vector<ResultField> fields;
    long elapsed;
    PgError error;
    wxString statusTag;
    unsigned long tuplesProcessedCount;
    Oid oidValue;
    bool tuplesProcessedCountValid;
    friend class ScriptEditor; // so, effectively all public...
    friend class ScriptQueryWork;
  };

  ScriptQueryWork(ScriptEditor *editor, const ExecutionLexer::Token &token) : editor(editor), token(token) {}

  void Execute() {
    const char *sql = editor->ExtractSQL(token);
    output = new Result(token);
    db->LogSql(sql);
    stopwatch.Start();

    PGresult *rs = PQexecParams(conn, sql, 0, NULL, NULL, NULL, NULL, 0);
    free((void*) sql);
    wxASSERT(rs != NULL);

    output->status = PQresultStatus(rs);
    if (output->status == PGRES_TUPLES_OK) {
      ReadColumns(rs);
      ReadResultSet(rs, output->data);
    }
    else if (output->status == PGRES_FATAL_ERROR) {
      db->LogSqlQueryFailed(PgError(rs));
      output->error = PgError(rs);
    }

    ReadStatus(rs);

    PQclear(rs);

    output->complete = true;

    output->elapsed = stopwatch.Time();
  }

  void NotifyFinished() {
    wxCommandEvent event(PQWX_ScriptQueryComplete);
    event.SetClientData(output);
    editor->AddPendingEvent(event);
  }

private:
  ScriptEditor *const editor;
  const ExecutionLexer::Token token;
  wxStopWatch stopwatch;
  Result *output;

  void ReadColumns(PGresult *rs)
  {
    int nfields = PQnfields(rs);
    for (int index = 0; index < nfields; index++) {
      output->fields.push_back(ResultField(rs, index));
    }
  }

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

};

#endif
