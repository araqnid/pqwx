// -*- c++ -*-

#ifndef __versioned_sql_h
#define __versioned_sql_h

#include <map>
#include <vector>
#include <algorithm>
#include "wx/string.h"

class VersionedSql {
protected:
  VersionedSql() {}
  void AddSql(const wxString &name, const char *sql, int majorVersion, int minorVersion) { AddSql(name, Statement(sql, majorVersion * 10000 + minorVersion * 100)); }
  void AddSql(const wxString &name, const char *sql) { AddSql(name, Statement(sql)); }
public:
  const char *GetSql(const wxString &name, int serverVersion) const {
    std::map<wxString, std::vector<Statement> >::const_iterator ptr = data.find(name);
    wxASSERT_MSG(ptr != data.end(), name);
    for (std::vector<Statement>::const_iterator iter = ptr->second.begin(); iter != ptr->second.end(); iter++) {
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
    data[name].push_back(stmt);
    sort(data[name].begin(), data[name].end());
    reverse(data[name].begin(), data[name].end());
  };
  std::map<wxString, std::vector<Statement> > data;
};

#endif
