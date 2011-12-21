// -*- c++ -*-

#ifndef __database_work_h
#define __database_work_h

#include <vector>
#include "libpq-fe.h"
#include "wx/string.h"
#include "wx/thread.h"
#include "sql_logger.h"

class DatabaseWork {
public:
  DatabaseWork() {}
  virtual ~DatabaseWork() {}

  virtual void Execute(PGconn *conn) = 0;
  virtual void NotifyFinished() = 0;

  // This tries to duplicate the fancy behaviour of quote_ident,
  // which avoids quoting identifiers when not necessary.
  // PQescapeIdentifier is "safe" in that it *always* quotes
  static wxString QuoteIdent(PGconn *conn, const wxString &str) {
    if (IsSimpleSymbol(str.utf8_str()))
      return str;
#if PG_VERSION_NUM >= 90000
    wxCharBuffer buf(str.utf8_str());
    char *escaped = PQescapeIdentifier(conn, buf.data(), strlen(buf.data()));
    wxString result = wxString::FromUTF8(escaped);
    PQfreemem(escaped);
#else
    wxString result;
    result.Alloc(str.length() + 2);
    result << _T('\"');
    for (int i = 0; i < str.length(); i++) {
      if (str[i] == '\"')
	result << _T("\"\"");
      else
	result << str[i];
    }
    result << _T('\"');
#endif
    return result;
  }

  static bool IsSimpleSymbol(const char *str) {
    for (const char *p = str; *p != '\0'; p++) {
      if (!( *p >= 'a' && *p <= 'z' || *p == '_' || *p >= '0' && *p <= '9' ))
	return false;
    }
    return true;
  }

#if PG_VERSION_NUM < 90000
  static bool standardConformingStrings(PGconn *conn) {
    const char *value = PQparameterStatus(conn, "standard_conforming_strings");
    return strcmp(value, "on") == 0;
  }
#endif

  static wxString QuoteLiteral(PGconn *conn, const wxString &str) {
#if PG_VERSION_NUM >= 90000
    wxCharBuffer buf(str.utf8_str());
    char *escaped = PQescapeLiteral(conn, buf.data(), strlen(buf.data()));
    wxString result = wxString::FromUTF8(escaped);
    PQfreemem(escaped);
#else
    wxString result;
    bool useEscapeSyntax = !standardConformingStrings(conn);
    bool usedEscapeSyntax = false;
    result.Alloc(str.length() + 2);
    result << _T('\'');
    for (int i = 0; i < str.length(); i++) {
      if (str[i] == '\'')
	result << _T("\'\'");
      else if (useEscapeSyntax && str[i] == '\\') {
	result << _T("\\\\");
	usedEscapeSyntax = true;
      }
      else
	result << str[i];
    }
    result << _T('\'');
    if (usedEscapeSyntax)
      result = _T('E') + result;
#endif
    return result;
  }

protected:
  bool DoCommand(PGconn *conn, const wxString &sql) {
    return DoCommand(conn, sql.utf8_str());
  }

  bool DoCommand(PGconn *conn, const char *sql) {
    logger->LogSql(sql);

    PGresult *rs = PQexec(conn, sql);
    wxASSERT(rs != NULL);

    ExecStatusType status = PQresultStatus(rs);
    if (status != PGRES_COMMAND_OK) {
      logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
      return false;
    }

    PQclear(rs);

    return true;
  }

  SqlLogger *logger;

  friend class DatabaseWorkerThread;
};

class DisconnectWork : public DatabaseWork {
public:
  class CloseCallback {
  public:
    virtual void OnConnectionClosed() = 0;
  };

  DisconnectWork(CloseCallback *callback = NULL) : callback(callback) {}
  void Execute(PGconn *conn) {
    PQfinish(conn);
    logger->LogDisconnect();
  }
private:
  CloseCallback *callback;
  void NotifyFinished() {
    if (callback)
      callback->OnConnectionClosed();
  }
};

class SnapshotIsolatedWork : virtual public DatabaseWork {
public:
  void Execute(PGconn *conn) {
    if (PQserverVersion(conn) >= 80000) {
      if (!DoCommand(conn, "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY"))
	return;
    }
    else {
      if (!DoCommand(conn, "BEGIN"))
	return;
      if (!DoCommand(conn, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE"))
	return;
      if (!DoCommand(conn, "SET TRANSACTION READ ONLY"))
	return;
    }

    ExecuteInTransaction(conn);

    DoCommand(conn, "END");
  }
  virtual void ExecuteInTransaction(PGconn *conn) = 0;
};

#endif
