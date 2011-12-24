// -*- c++ -*-

#ifndef __database_work_h
#define __database_work_h

#include <vector>
#include "libpq-fe.h"
#include "wx/string.h"
#include "wx/thread.h"
#include "sql_logger.h"
#include "query_results.h"
#include "versioned_sql.h"

class DatabaseWork {
public:
  DatabaseWork(const VersionedSql *sqlDictionary = NULL) : sqlDictionary(sqlDictionary) {}
  virtual ~DatabaseWork() {}

  virtual void Execute() = 0;
  virtual void NotifyFinished() = 0;

  // This tries to duplicate the fancy behaviour of quote_ident,
  // which avoids quoting identifiers when not necessary.
  // PQescapeIdentifier is "safe" in that it *always* quotes
  wxString QuoteIdent(const wxString &str) {
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
  bool standardConformingStrings() {
    const char *value = PQparameterStatus(conn, "standard_conforming_strings");
    return strcmp(value, "on") == 0;
  }
#endif

  wxString QuoteLiteral(const wxString &str) {
#if PG_VERSION_NUM >= 90000
    wxCharBuffer buf(str.utf8_str());
    char *escaped = PQescapeLiteral(conn, buf.data(), strlen(buf.data()));
    wxString result = wxString::FromUTF8(escaped);
    PQfreemem(escaped);
#else
    wxString result;
    bool useEscapeSyntax = !standardConformingStrings();
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

  bool DoCommand(const wxString &sql) { return DoCommand(sql.utf8_str()); }
  bool DoCommand(const char *sql);
  bool DoQuery(const char *sql, QueryResults &rs);
  bool DoQuery(const char *sql, QueryResults &rs, Oid paramType, const char *paramValue);
  bool DoQuery(const char *sql, QueryResults &rs, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value);

  bool DoNamedCommand(const wxString &name) { return DoCommand(sqlDictionary->GetSql(name, PQserverVersion(conn))); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs) { return DoQuery(sqlDictionary->GetSql(name, PQserverVersion(conn)), rs); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs, Oid paramType, const char *paramValue) { return DoQuery(sqlDictionary->GetSql(name, PQserverVersion(conn)), rs, paramType, paramValue); }
  bool DoNamedQuery(const wxString &name, QueryResults &rs, Oid param1Type, Oid param2Type, const char *param1Value, const char *param2Value) { return DoQuery(sqlDictionary->GetSql(name, PQserverVersion(conn)), rs, param1Type, param2Type, param1Value, param2Value); }

  PGconn *conn;
  SqlLogger *logger;
  const VersionedSql *sqlDictionary;

private:
  void ReadResultSet(PGresult *rs, QueryResults &results);

  friend class DatabaseWorkerThread;
};

class DisconnectWork : public DatabaseWork {
public:
  class CloseCallback {
  public:
    virtual void OnConnectionClosed() = 0;
  };

  DisconnectWork(CloseCallback *callback = NULL) : callback(callback) {}
  void Execute() {
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

#endif
