/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __versioned_sql_h
#define __versioned_sql_h

#include <map>
#include <set>
#include <algorithm>
#include "wx/string.h"

/**
 * Associates name strings with SQL statements, with variations based on server version.
 *
 * Generally referred to everywhere else as a "SQL dictionary", another misnomer.
 *
 * This class has no public constructors. Typically, subclasses are
 * generated from input files that populate themselves in a public
 * no-arg constructor.
 */
class VersionedSql {
protected:
  /**
   * Create an empty dictionary.
   */
  VersionedSql() {}
  /**
   * Add a SQL statement requiring a particular minimum server version.
   */
  void AddSql(const wxString &name, const char *sql, int majorVersion, int minorVersion) { AddSql(name, Statement(sql, majorVersion * 10000 + minorVersion * 100)); }
  /**
   * Add a default SQL statement.
   */
  void AddSql(const wxString &name, const char *sql) { AddSql(name, Statement(sql)); }
public:
  /**
   * Gets the SQL text for a specified name and server version.
   */
  const char *GetSql(const wxString &name, int serverVersion) const {
    std::map<wxString, std::set<Statement> >::const_iterator ptr = data.find(name);
    wxASSERT_MSG(ptr != data.end(), name);
    for (std::set<Statement>::const_reverse_iterator iter = ptr->second.rbegin(); iter != ptr->second.rend(); iter++) {
      if (serverVersion >= iter->minVersion) {
	return iter->sql;
      }
    }
    wxFAIL_MSG(name);
    return NULL;
  }
private:
  static const int MIN_VERSION = 0;
  class Statement {
  public:
    Statement(const char *sql, int minVersion = MIN_VERSION) : sql(sql), minVersion(minVersion) {}
    const char *sql;
    int minVersion;
    bool operator< (const Statement &other) const {
      return minVersion < other.minVersion;
    }
  };
  void AddSql(const wxString &name, Statement stmt) {
    data[name].insert(stmt);
  };
  std::map<wxString, std::set<Statement> > data;
};

#endif

// Local Variables:
// mode: c++
// End:
