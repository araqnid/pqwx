/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __pg_error_h
#define __pg_error_h

#include <vector>
#include "libpq-fe.h"
#include "wx/tokenzr.h"

/**
 * Represents an error from the database server.
 */
class PgError {
public:
  PgError() {}
  PgError(const PGresult *rs)
  {
    severity = GetErrorField(rs, PG_DIAG_SEVERITY);
    sqlstate = GetErrorField(rs, PG_DIAG_SQLSTATE);
    primary = GetErrorField(rs, PG_DIAG_MESSAGE_PRIMARY);
    detail = GetErrorField(rs, PG_DIAG_MESSAGE_DETAIL);
    hint = GetErrorField(rs, PG_DIAG_MESSAGE_HINT);
    position = GetErrorIntegerField(rs, PG_DIAG_STATEMENT_POSITION, -1);
    internalPosition = GetErrorIntegerField(rs, PG_DIAG_INTERNAL_POSITION, -1);
    internalQuery = GetErrorField(rs, PG_DIAG_INTERNAL_QUERY);
    sourceFile = GetErrorField(rs, PG_DIAG_SOURCE_FILE);
    sourceLine = GetErrorIntegerField(rs, PG_DIAG_SOURCE_LINE);
    sourceFunction = GetErrorField(rs, PG_DIAG_SOURCE_FUNCTION);
    context = GetErrorMultilineField(rs, PG_DIAG_CONTEXT);
  }
  PgError(const PGconn *conn)
  {
    severity = _T("ERROR");
    primary = wxString(PQerrorMessage(conn), wxConvUTF8);
  }

  wxString GetPrimary() const { return primary; }
  wxString GetSeverity() const { return severity; }
  wxString GetHint() const { return hint; }
  bool HasHint() const { return !hint.empty(); }
  wxString GetDetail() const { return detail; }
  bool HasDetail() const { return !detail.empty(); }
  const std::vector<wxString>& GetContext() const { return context; }
  bool HasContext() const { return !context.empty(); }
  int GetPosition() const { return position; }
  bool HasPosition() const { return position >= 0; }

private:
  static wxString GetErrorField(const PGresult *rs, int fieldCode)
  {
    const char *message = PQresultErrorField(rs, fieldCode);
    if (message == NULL) return wxEmptyString;
    return wxString(message, wxConvUTF8);
  }

  static std::vector<wxString> GetErrorMultilineField(const PGresult *rs, int fieldCode)
  {
    std::vector<wxString> lines;
    const char *message = PQresultErrorField(rs, fieldCode);
    if (message != NULL) {
      wxStringTokenizer tkz(wxString(message, wxConvUTF8), _T("\n"));
      while (tkz.HasMoreTokens()) {
	lines.push_back(tkz.GetNextToken());
      }
    }
    return lines;
  }

  static int GetErrorIntegerField(const PGresult *rs, int fieldCode, int rebase = 0)
  {
    const char *message = PQresultErrorField(rs, fieldCode);
    if (message == NULL) return -1;
    long value;
    wxCHECK(wxString(message, wxConvUTF8).ToLong(&value), -1);
    return (int) value + rebase;
  }

  wxString severity, sqlstate, primary, detail, hint, 
    internalQuery, sourceFile, sourceFunction;
  int position, internalPosition, sourceLine;
  std::vector<wxString> context;
};

#endif

// Local Variables:
// mode: c++
// End:
