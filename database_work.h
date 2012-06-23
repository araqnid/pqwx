/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __database_work_h
#define __database_work_h

#include <vector>
#include "libpq-fe.h"
#include "wx/string.h"
#include "wx/thread.h"
#include "query_results.h"
#include "versioned_sql.h"
#include "database_connection.h"

/**
 * Encapsulates a unit of work to be performed on a database connection.
 */
class DatabaseWork {
public:
  DatabaseWork() {}
  virtual ~DatabaseWork() {}

  virtual void operator()() = 0;
  virtual void NotifyFinished() = 0;
  virtual void NotifyCrashed(const std::exception& e) { NotifyCrashed(); }
  virtual void NotifyCrashed() {}

  wxString QuoteIdent(const wxString &str) const;
  wxString QuoteLiteral(const wxString &str) const;

  bool DoCommand(const wxString &sql) const { return DoCommand((const char*) sql.utf8_str()); }
  bool DoCommand(const char *sql) const;

  QueryResults DoQuery(const char *sql, int paramCount, const Oid *paramTypes, const char **paramValues) const;
  QueryResults DoQuery(const char *sql, std::vector<Oid> const& paramTypes, std::vector<wxString> const& paramValues) const;

  QueryResults DoNamedQuery(const wxString &name, const char *sql, int paramCount, const Oid *paramTypes, const char **paramValues) const;
  QueryResults DoNamedQuery(const wxString &name, const char *sql, std::vector<Oid> const& paramTypes, std::vector<wxString> const& paramValues) const;

  /**
   * Fluent-style query executor class.
   */
  class QueryExecutor {
  public:
    QueryExecutor(const DatabaseWork *owner, const char *sql) : owner(owner), sql(sql) {}

    /**
     * Add parameter with string value.
     */
    QueryExecutor& Param(Oid type, const wxString& value) {
      paramTypes.push_back(type);
      paramValues.push_back(value);
      return *this;
    }

    /**
     * Add OID parameter.
     */
    QueryExecutor& OidParam(Oid value) {
      return Param(26 /*oid*/, wxString::Format(_T("%u"), value));
    }

    /**
     * Execute for result set.
     */
    QueryResults List() {
      return owner->DoQuery(sql, paramTypes, paramValues);
    }

    /**
     * Execute for single row.
     */
    QueryResults::Row UniqueResult() {
      QueryResults rs = List();
      wxASSERT(rs.size() == 1);
      return rs[0];
    }
 private:
    const DatabaseWork *owner;
    const char *sql;
    std::vector<Oid> paramTypes;
    std::vector<wxString> paramValues;
  };

  /**
   * Execute query.
   *
   * Call as:
   * <code>Query("SELECT * FROM pg_database WHERE oid = $1").Param(26, oid).UniqueResult()</code>
   */
  QueryExecutor Query(const char *sql) const {
    return QueryExecutor(this, sql);
  }

protected:
  DatabaseConnection *db;
  PGconn *conn;

  friend class DatabaseConnection::WorkerThread;
};

/**
 * Some database work with a SQL dictionary.
 */
class DatabaseWorkWithDictionary : public DatabaseWork {
public:
  DatabaseWorkWithDictionary(const VersionedSql &sqlDictionary) : sqlDictionary(sqlDictionary) {}

  /**
   * Execute named command.
   */
  bool DoCommand(const wxString &name) const { return DatabaseWork::DoCommand(GetSql(name)); }
  /**
   * Execute named query with array parameters.
   */
  QueryResults DoQuery(const wxString &name, int paramCount, Oid paramTypes[], const char *paramValues[]) const
  {
    return DatabaseWork::DoNamedQuery(name, GetSql(name), paramCount, paramTypes, paramValues);
  }
  /**
   * Execute named query with vector parameters.
   */
  QueryResults DoQuery(const wxString &name, const std::vector<Oid>& paramTypes, const std::vector<wxString>& paramValues) const
  {
    if (paramTypes.empty()) return DatabaseWork::DoNamedQuery(name, GetSql(name), 0, NULL, NULL);
    return DatabaseWork::DoNamedQuery(name, GetSql(name), paramTypes, paramValues);
  }
  /**
   * Get SQL from dictionary.
   */
  const char *GetSql(const wxString &name) const
  {
    return sqlDictionary.GetSql(name, PQserverVersion(conn));
  }

  /**
   * Fluent-style named query executor class.
   */
  class NamedQueryExecutor {
  public:
    NamedQueryExecutor(const DatabaseWorkWithDictionary *owner, const wxString& name) : owner(owner), name(name) {}

    /**
     * Add a parameter with a string value.
     */
    NamedQueryExecutor& Param(Oid type, const wxString& value) {
      paramTypes.push_back(type);
      paramValues.push_back(value);
      return *this;
    }

    /**
     * Add an OID parameter.
     */
    NamedQueryExecutor& OidParam(Oid value) {
      return Param(26 /*oid*/, wxString::Format(_T("%u"), value));
    }

    /**
     * Execute query and return result set.
     */
    QueryResults List() {
      return owner->DoQuery(name, paramTypes, paramValues);
    }

    /**
     * Execute query and return exactly one row.
     */
    QueryResults::Row UniqueResult() {
      QueryResults rs = List();
      wxASSERT(rs.size() == 1);
      return rs[0];
    }

 private:
    const DatabaseWorkWithDictionary *owner;
    wxString name;
    std::vector<Oid> paramTypes;
    std::vector<wxString> paramValues;
  };

  /**
   * Execute a query.
   *
   * Call as:
   * <code>Query(_T("QueryName")).Param(26, 31337).List()</code>
   */
  NamedQueryExecutor Query(const wxString &name) const {
    return NamedQueryExecutor(this, name);
  }

private:
  const VersionedSql& sqlDictionary;
};

#endif

// Local Variables:
// mode: c++
// End:
