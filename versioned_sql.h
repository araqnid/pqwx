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
  const char *GetSql(const wxString &name, int serverVersion) {
    wxASSERT_MSG(data.count(name) > 0, name);
    for (std::vector<Statement>::iterator iter = data[name].begin(); iter != data[name].end(); iter++) {
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
