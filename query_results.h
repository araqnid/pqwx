// -*- mode: c++ -*-

#ifndef __query_results_h
#define __query_results_h

#include <vector>
#include "libpq-fe.h"
#include "wx/string.h"
#include "pg_error.h"

class QueryResults {
public:
  class Field {
  public:
    Field(PGresult *rs, int index) {
      name = wxString(PQfname(rs, index), wxConvUTF8);
      table = PQftable(rs, index);
      if (table != InvalidOid) tableColumn = PQftablecol(rs, index);
      type = PQftype(rs, index);
      typemod = PQfmod(rs, index);
    }

    const wxString& GetName() const { return name; }
    Oid GetType() const { return type; }
  private:
    wxString name;
    Oid table;
    int tableColumn;
    Oid type;
    int typemod;

    friend class QueryResults;
  };

  class Row {
  public:
    Row(const PGresult *rs, const QueryResults *owner, int rowNum) : owner(owner) {
      int colCount = PQnfields(rs);
      data.reserve(colCount);
      for (int colNum = 0; colNum < colCount; colNum++) {
	const char *value = PQgetvalue(rs, rowNum, colNum);
	data.push_back(wxString(value, wxConvUTF8));
      }
    }

    const wxString& operator[](unsigned index) const {
      wxASSERT(index < data.size());
      return data[index];
    }
    const wxString& operator[](const wxString &fieldName) const { return data[owner->GetFieldNumber(fieldName)]; }
    unsigned size() const { return data.size(); }

    Oid ReadOid(unsigned index) const {
      wxASSERT(index < data.size());
      long value;
      wxCHECK(data[index].ToLong(&value), 0);
      return value;
    }

    bool ReadBool(unsigned index) const {
      wxASSERT(index < data.size());
      return data[index] == _T("t");
    }

    const wxString& ReadText(unsigned index) const {
      wxASSERT(index < data.size());
      return data[index];
    }

    wxInt32 ReadInt4(unsigned index) const {
      wxASSERT(index < data.size());
      long value;
      wxCHECK(data[index].ToLong(&value), (wxInt32)0);
      return (wxInt32) value;
    }

    wxInt64 ReadInt8(unsigned index) const {
      wxASSERT(index < data.size());
      long value;
      wxCHECK(data[index].ToLong(&value), (wxInt64)0);
      return (wxInt64) value;
    }

  private:
    std::vector<wxString> data;
    const QueryResults *owner;
  };

  QueryResults(PGresult *rs) {
    int rowCount = PQntuples(rs);
    int colCount = PQnfields(rs);

    fields.reserve(colCount);
    for (int i = 0; i < colCount; i++) {
      fields.push_back(Field(rs, i));
    }

    rows.reserve(rowCount);
    for (int rowNum = 0; rowNum < rowCount; rowNum++) {
      rows.push_back(Row(rs, this, rowNum));
    }
  }

  typedef std::vector<Row>::const_iterator const_iterator;
  const_iterator begin() const { return rows.begin(); }
  const_iterator end() const { return rows.end(); }
  const Row& operator[](int index) const { return rows[index]; }
  unsigned size() const { return rows.size(); }

  const std::vector<Field>& Fields() const { return fields; }

  const Field& GetField(const wxString &fieldName) const {
    for (std::vector<Field>::const_iterator iter = fields.begin(); iter != fields.end(); iter++) {
      if ((*iter).name == fieldName) return *iter;
    }
    wxASSERT_MSG(false, fieldName);
  }

  int GetFieldNumber(const wxString &fieldName) const {
    int pos = 0;
    for (std::vector<Field>::const_iterator iter = fields.begin(); iter != fields.end(); iter++, pos++) {
      if ((*iter).name == fieldName) return pos;
    }
    wxASSERT_MSG(false, fieldName);
  }

private:
  std::vector<Row> rows;
  std::vector<Field> fields;
};

class PgResourceFailure : public std::exception {
};

class PgQueryFailure : public std::exception {
public:
  PgQueryFailure(const PgError &error) : error(error) {}
  virtual ~PgQueryFailure() throw () {}
  const PgError& GetDetails() const { return error; }
private:
  PgError error;
};

class PgInvalidQuery : public std::exception {
public:
  PgInvalidQuery(const wxString &message) : message(message) {}
  virtual ~PgInvalidQuery() throw () {}
  const wxString& GetMessage() const { return message; }
private:
  wxString message;
};

#endif
