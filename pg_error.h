// -*- mode: c++ -*-

#ifndef __pg_error_h
#define __pg_error_h

class PgError {
public:
  PgError() {}
  PgError(const PGresult *rs) {
    severity = GetErrorField(rs, PG_DIAG_SEVERITY);
    sqlstate = GetErrorField(rs, PG_DIAG_SQLSTATE);
    primary = GetErrorField(rs, PG_DIAG_MESSAGE_PRIMARY);
    detail = GetErrorField(rs, PG_DIAG_MESSAGE_DETAIL);
    hint = GetErrorField(rs, PG_DIAG_MESSAGE_HINT);
    position = GetErrorField(rs, PG_DIAG_STATEMENT_POSITION);
    internalPosition = GetErrorField(rs, PG_DIAG_INTERNAL_POSITION);
    internalQuery = GetErrorField(rs, PG_DIAG_INTERNAL_QUERY);
    sourceFile = GetErrorField(rs, PG_DIAG_SOURCE_FILE);
    sourceLine = GetErrorField(rs, PG_DIAG_SOURCE_LINE);
    sourceFunction = GetErrorField(rs, PG_DIAG_SOURCE_FUNCTION);
  }

private:
  static wxString GetErrorField(const PGresult *rs, int fieldCode)
  {
    return wxString(PQresultErrorField(rs, fieldCode), wxConvUTF8);
  }

  wxString severity, sqlstate, primary, detail, hint, position,
    internalPosition, internalQuery, sourceFile, sourceLine, sourceFunction;

  friend class ScriptEditor;
  friend class DatabaseConnection;
  friend class ResultsNotebook;
};

#endif
