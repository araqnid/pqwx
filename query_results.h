// -*- mode: c++ -*-

#ifndef __query_results_h
#define __query_results_h

#include <vector>
#include "libpq-fe.h"
#include "wx/string.h"

class ResultField {
public:
  ResultField(PGresult *rs, int index) {
    name = wxString(PQfname(rs, index), wxConvUTF8);
    table = PQftable(rs, index);
    if (table != InvalidOid) tableColumn = PQftablecol(rs, index);
    type = PQftype(rs, index);
    typemod = PQfmod(rs, index);
  }
private:
  wxString name;
  Oid table;
  int tableColumn;
  Oid type;
  int typemod;

  friend class ResultsNotebook;
};

typedef std::vector<wxString> QueryRow;
typedef std::vector<QueryRow> QueryResults;

static inline Oid ReadOid(const std::vector<wxString> &row, unsigned index) {
  wxASSERT(index < row.size());
  long value;
  wxCHECK(row[index].ToLong(&value), 0);
  return value;
}

static inline Oid ReadOid(QueryResults::iterator iter, unsigned index) {
  return ReadOid(*iter, index);
}

static inline bool ReadBool(const std::vector<wxString> &row, unsigned index) {
  wxASSERT(index < row.size());
  return row[index] == _T("t");
}

static inline bool ReadBool(QueryResults::iterator iter, unsigned index) {
  return ReadBool(*iter, index);
}

static inline wxString ReadText(const std::vector<wxString> &row, unsigned index) {
  wxASSERT(index < row.size());
  return row[index];
}

static inline wxString ReadText(QueryResults::iterator iter, unsigned index) {
  return ReadText(*iter, index);
}

static inline wxInt32 ReadInt4(const std::vector<wxString> &row, unsigned index) {
  wxASSERT(index < row.size());
  long value;
  wxCHECK(row[index].ToLong(&value), (wxInt32)0);
  return (wxInt32) value;
}

static inline wxInt32 ReadInt4(QueryResults::iterator iter, unsigned index) {
  return ReadInt4(*iter, index);
}

static inline wxInt64 ReadInt8(const std::vector<wxString> &row, unsigned index) {
  wxASSERT(index < row.size());
  long value;
  wxCHECK(row[index].ToLong(&value), (wxInt64)0);
  return (wxInt64) value;
}

static inline wxInt64 ReadInt8(QueryResults::iterator iter, unsigned index) {
  return ReadInt8(*iter, index);
}

#endif
