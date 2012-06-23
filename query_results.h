/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __query_results_h
#define __query_results_h

#include <vector>
#include "libpq-fe.h"
#include "wx/string.h"
#include "pg_error.h"

/**
 * Simple query result set representation.
 *
 * This is a translation of a libpq result set to use wxStrings and
 * STL containers, for ease of use. It is mainly intended for the
 * object browser, which tends to return reasonably-sized result sets.
 */
class QueryResults {
public:
  /**
   * A field (column description) in the result set.
   */
  class Field {
  public:
    /**
     * Create field for libpq result and column index.
     */
    Field(PGresult *rs, int index) {
      name = wxString(PQfname(rs, index), wxConvUTF8);
      table = PQftable(rs, index);
      if (table != InvalidOid) tableColumn = PQftablecol(rs, index);
      type = PQftype(rs, index);
      typemod = PQfmod(rs, index);
    }

    /**
     * @return field name
     */
    const wxString& GetName() const { return name; }
    /**
     * @return field type
     */
    Oid GetType() const { return type; }
  private:
    wxString name;
    Oid table;
    int tableColumn;
    Oid type;
    int typemod;

    friend class QueryResults;
  };

  /**
   * A result set row.
   */
  class Row {
  public:
    /**
     * Create row from libpq result set and row number, with reference to owning result set.
     * The parent result set is used to resolve fields by name.
     */
    Row(const PGresult *rs, const QueryResults *owner, int rowNum) : owner(owner) {
      int colCount = PQnfields(rs);
      data.reserve(colCount);
      for (int colNum = 0; colNum < colCount; colNum++) {
	const char *value = PQgetvalue(rs, rowNum, colNum);
	data.push_back(wxString(value, wxConvUTF8));
      }
    }

    /**
     * Get string value by field index.
     */
    const wxString& operator[](unsigned index) const {
      wxASSERT(index < data.size());
      return data[index];
    }
    /**
     * Get string value by field name.
     */
    const wxString& operator[](const wxString &fieldName) const { return data[owner->GetFieldNumber(fieldName)]; }
    /**
     * @return number of fields in row.
     */
    unsigned size() const { return data.size(); }

    /**
     * Get OID value by field index.
     */
    Oid ReadOid(unsigned index) const {
      wxASSERT(index < data.size());
      long value;
      wxCHECK(data[index].ToLong(&value), 0);
      return value;
    }

    /**
     * Get boolean value by field index.
     */
    bool ReadBool(unsigned index) const {
      wxASSERT(index < data.size());
      return data[index] == _T("t");
    }

    /**
     * Get boolean value by field name.
     */
    bool ReadBool(const wxString& fieldName) const {
      return data[owner->GetFieldNumber(fieldName)] == _T("t");
    }

    /**
     * Get string value by field index.
     * This is basically identical to operator[](unsigned)
     */
    const wxString& ReadText(unsigned index) const {
      wxASSERT(index < data.size());
      return data[index];
    }

    /**
     * Get int4 value by field index.
     */
    wxInt32 ReadInt4(unsigned index) const {
      wxASSERT(index < data.size());
      long value;
      wxCHECK(data[index].ToLong(&value), (wxInt32)0);
      return (wxInt32) value;
    }

    /**
     * Get int8 value by field index.
     */
    wxInt64 ReadInt8(unsigned index) const {
      wxASSERT(index < data.size());
      long long value;
      wxCHECK(data[index].ToLongLong(&value), (wxInt64)0);
      return (wxInt64) value;
    }

  private:
    std::vector<wxString> data;
    const QueryResults *owner;
  };

  /**
   * Create query results from libpq result.
   */
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

  /** Immutable iterator */
  typedef std::vector<Row>::const_iterator const_iterator;
  /** Start of iteration space */
  const_iterator begin() const { return rows.begin(); }
  /** End of iteration space */
  const_iterator end() const { return rows.end(); }
  /**
   * Gets row by row index.
   */
  const Row& operator[](int index) const { return rows[index]; }
  /**
   * @return Number of rows
   */
  unsigned size() const { return rows.size(); }

  /**
   * @return Fields (column descriptions)
   */
  const std::vector<Field>& Fields() const { return fields; }

  /**
   * Get field by name.
   * In debug mode, traps if field not found.
   */
  const Field& GetField(const wxString &fieldName) const {
    for (std::vector<Field>::const_iterator iter = fields.begin(); iter != fields.end(); iter++) {
      if ((*iter).name == fieldName) return *iter;
    }
    wxASSERT_MSG(false, fieldName);
  }

  /**
   * Convert field name to number.
   * In debug mode, traps if field not found.
   */
  int GetFieldNumber(const wxString &fieldName) const {
    int pos = 0;
    for (std::vector<Field>::const_iterator iter = fields.begin(); iter != fields.end(); iter++, pos++) {
      if ((*iter).name == fieldName) return pos;
    }
    wxASSERT_MSG(false, fieldName);
    return -1; // quiet gcc
  }

private:
  std::vector<Row> rows;
  std::vector<Field> fields;
};

/**
 * Base class included by exceptions that retain which query caused them.
 */
class PgQueryRelatedException {
public:
  PgQueryRelatedException(const wxString &queryName_) : queryName(queryName_), sql() {}
  PgQueryRelatedException(const char *sql_) : queryName(), sql(wxString(sql_, wxConvUTF8)) {}
  bool IsFromNamedQuery() const { return !queryName.empty(); }
  const wxString& GetQueryName() const { return queryName; }
  const wxString& GetSql() const { return sql; }
private:
  const wxString queryName;
  const wxString sql;
};

/**
 * Exception thrown when libpq was unable to return a result due to out of memory.
 */
class PgResourceFailure : public std::exception {
};

/**
 * Execption thrown echoing a server error.
 */
class PgQueryFailure : public std::exception, public PgQueryRelatedException {
public:
  PgQueryFailure(const wxString &name, const PgError &error_) : PgQueryRelatedException(name), error(error_), messageBuf(error.GetPrimary().utf8_str()) {}
  PgQueryFailure(const char *sql, const PgError &error_) : PgQueryRelatedException(sql), error(error_), messageBuf(error.GetPrimary().utf8_str()) {}
  virtual ~PgQueryFailure() throw () {}
  const PgError& GetDetails() const { return error; }
  const char *what() const throw () { return messageBuf.data(); }
private:
  PgError error;
  wxCharBuffer messageBuf;
};

/**
 * Exception thrown when query execution produced an invalid state.
 *
 * For example, if a query that was expected to return tuples did not or vice versa.
 */
class PgInvalidQuery : public std::exception, public PgQueryRelatedException {
public:
  PgInvalidQuery(const wxString &name, const wxString &message) : PgQueryRelatedException(name), message(message), messageBuf(message.utf8_str()) {}
  PgInvalidQuery(const char *sql, const wxString &message) : PgQueryRelatedException(sql), message(message), messageBuf(message.utf8_str()) {}
  virtual ~PgInvalidQuery() throw () {}
  const wxString& GetMessage() const throw () { return message; }
  const char *what() const throw () { return messageBuf.data(); }
private:
  wxString message;
  wxCharBuffer messageBuf;
};

#endif

// Local Variables:
// mode: c++
// End:
