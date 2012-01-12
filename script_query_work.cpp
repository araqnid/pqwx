#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "script_query_work.h"
#include "results_display.h"

void ScriptExecutionWork::Result::ReadStatus(DatabaseConnection *db, PGconn *conn, PGresult *rs)
{
  status = PQresultStatus(rs);
  if (status == PGRES_TUPLES_OK) {
    data = std::auto_ptr<QueryResults>(new QueryResults(rs));
    newConnectionState = Idle;
  }
  else if (status == PGRES_FATAL_ERROR) {
    error = PgError(rs);
    db->LogSqlQueryFailed(error);
    newConnectionState = Idle;
  }
  else if (status == PGRES_COMMAND_OK) {
    newConnectionState = Idle;
  }
  else if (status == PGRES_COPY_OUT) {
    newConnectionState = CopyToClient;
  }
  else if (status == PGRES_COPY_IN) {
    newConnectionState = CopyToServer;
  }
  else if (status == PGRES_EMPTY_QUERY) {
    newConnectionState = Idle;
  }
  statusTag = wxString(PQcmdStatus(rs), wxConvUTF8);

  wxString tuplesCount = wxString(PQcmdTuples(rs), wxConvUTF8);
  if (tuplesCount.empty())
    tuplesProcessedCountValid = false;
  else {
    tuplesProcessedCountValid = tuplesCount.ToULong(&tuplesProcessedCount);
  }
  oidValue = PQoidValue(rs);
}

void ScriptQueryWork::operator()()
{
  wxStopWatch stopwatch;
  wxCharBuffer sqlBuffer = sql.utf8_str();

  output = new Result();
  db->LogSql(sqlBuffer.data());
  stopwatch.Start();

  PGresult *rs = PQexecParams(conn, sqlBuffer.data(), 0, NULL, NULL, NULL, NULL, 0);
  wxASSERT(rs != NULL);

  output->ReadStatus(db, conn, rs);

  PQclear(rs);

  output->Finalise(stopwatch.Time(), conn);
}

void ScriptPutCopyDataWork::operator()()
{
  wxStopWatch stopwatch;

  output = new Result();

  wxLogDebug(_T("Putting %u bytes of COPY data"), token.length);

  int rc = PQputCopyData(conn, buffer + token.offset, token.length);
  if (rc < 0) {
    wxLogDebug(_T("PQputCopyData failed"));
    output->error = PgError(conn);
    return;
  }

  wxLogDebug(_T("Finishing COPY"));

  rc = PQputCopyEnd(conn, NULL);
  if (rc < 0) {
    wxLogDebug(_T("PQputCopyEnd failed"));
    output->error = PgError(conn);
    return;
  }

  wxLogDebug(_T("Getting statement result"));

  PGresult *rs;
  do {
    rs = PQgetResult(conn);
    if (rs != NULL) {
      output->ReadStatus(db, conn, rs);
    }
  } while (rs != NULL);

  PQclear(rs);

  output->Finalise(stopwatch.Time(), conn);
}
// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
