// -*- c++ -*-

#ifndef __versioned_sql_h
#define __versioned_sql_h

#include <map>
#include <set>
#include <algorithm>
#include "wx/string.h"

class VersionedSql {
protected:
  VersionedSql() {}
  void AddSql(const wxString &name, const char *sql, int majorVersion, int minorVersion) { AddSql(name, Statement(sql, majorVersion * 10000 + minorVersion * 100)); }
  void AddSql(const wxString &name, const char *sql) { AddSql(name, Statement(sql)); }
public:
  const char *GetSql(const wxString &name, int serverVersion) const {
    std::map<wxString, std::set<Statement> >::const_iterator ptr = data.find(name);
    wxASSERT_MSG(ptr != data.end(), name);
    for (std::set<Statement>::const_reverse_iterator iter = ptr->second.rbegin(); iter != ptr->second.rend(); iter++) {
      if (serverVersion >= iter->minVersion) {
	return iter->sql;
      }
    }
    wxFAIL_MSG(name);
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
