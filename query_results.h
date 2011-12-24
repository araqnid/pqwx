// -*- mode: c++ -*-

#ifndef __query_results_h
#define __query_results_h

typedef std::vector<wxString> QueryRow;
typedef std::vector<QueryRow> QueryResults;

static inline Oid ReadOid(QueryResults::iterator iter, int index) {
  wxASSERT(index < (*iter).size());
  long value;
  wxCHECK((*iter)[index].ToLong(&value), 0);
  return value;
}

static inline bool ReadBool(QueryResults::iterator iter, int index) {
  wxASSERT(index < (*iter).size());
  return (*iter)[index].IsSameAs(_T("t"));
}

static inline wxString ReadText(QueryResults::iterator iter, int index) {
  wxASSERT(index < (*iter).size());
  return (*iter)[index];
}

static inline wxInt32 ReadInt4(QueryResults::iterator iter, int index) {
  wxASSERT(index < (*iter).size());
  long value;
  wxCHECK((*iter)[index].ToLong(&value), (wxInt32)0);
  return (wxInt32) value;
}

static inline wxInt64 ReadInt8(QueryResults::iterator iter, int index) {
  wxASSERT(index < (*iter).size());
  long value;
  wxCHECK((*iter)[index].ToLong(&value), (wxInt64)0);
  return (wxInt64) value;
}

#endif
