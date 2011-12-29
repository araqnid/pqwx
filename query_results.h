// -*- mode: c++ -*-

#ifndef __query_results_h
#define __query_results_h

#include <vector>
#include "libpq-fe.h"
#include "wx/string.h"

typedef std::vector<wxString> QueryRow;
typedef std::vector<QueryRow> QueryResults;

static inline Oid ReadOid(const std::vector<wxString> &row, int index) {
  wxASSERT(index < row.size());
  long value;
  wxCHECK(row[index].ToLong(&value), 0);
  return value;
}

static inline Oid ReadOid(QueryResults::iterator iter, int index) {
  return ReadOid(*iter, index);
}

static inline bool ReadBool(const std::vector<wxString> &row, int index) {
  wxASSERT(index < row.size());
  return row[index].IsSameAs(_T("t"));
}

static inline bool ReadBool(QueryResults::iterator iter, int index) {
  return ReadBool(*iter, index);
}

static inline wxString ReadText(const std::vector<wxString> &row, int index) {
  wxASSERT(index < row.size());
  return row[index];
}

static inline wxString ReadText(QueryResults::iterator iter, int index) {
  return ReadText(*iter, index);
}

static inline wxInt32 ReadInt4(const std::vector<wxString> &row, int index) {
  wxASSERT(index < row.size());
  long value;
  wxCHECK(row[index].ToLong(&value), (wxInt32)0);
  return (wxInt32) value;
}

static inline wxInt32 ReadInt4(QueryResults::iterator iter, int index) {
  return ReadInt4(*iter, index);
}

static inline wxInt64 ReadInt8(const std::vector<wxString> &row, int index) {
  wxASSERT(index < row.size());
  long value;
  wxCHECK(row[index].ToLong(&value), (wxInt64)0);
  return (wxInt64) value;
}

static inline wxInt64 ReadInt8(QueryResults::iterator iter, int index) {
  ReadInt8(*iter, index);
}

#endif
