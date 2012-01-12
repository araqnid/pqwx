/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __results_display_h
#define __results_display_h

#include "libpq-fe.h"
#include <stdexcept>
#include <cstring>
#include <sys/mman.h>

namespace PQWX {

  template<typename T>
  class auto_array {
  public:
    auto_array() : ptr(NULL) {}
    auto_array(T *ptr) : ptr(ptr) {}
    auto_array(auto_array& other)
    {
      if (other.ptr == NULL) {
	ptr = NULL;
      }
      else {
	ptr = other.ptr;
	other.ptr = NULL;
      }
    }
    ~auto_array() { deallocate(); }

    auto_array& operator=(T *ptr_)
    {
      ptr_ = ptr;
      return *this;
    }

    auto_array& operator=(auto_array& other)
    {
      if (other.ptr == NULL) {
	if (ptr) deallocate();
	ptr = NULL;
      }
      else if (&other != this) {
	if (ptr) deallocate();
	ptr = other.ptr;
	other.ptr = NULL;
      }
      return *this;
    }

    T& operator*() { return *ptr; }
    T& operator[](unsigned n) { return ptr[n]; }
    const T& operator[](unsigned n) const { return ptr[n]; }
    T* operator->() { return ptr; }
    T* operator+(int offset) { return ptr + offset; }
    const T* operator+(int offset) const { return ptr + offset; }

    void release() { ptr = NULL; }

  private:
    void deallocate()
    {
      delete[] ptr;
    }

    T *ptr;
  };

};

class ResultGridData {
public:
  virtual ~ResultGridData() {}

  /**
   * Indicate the number of rows.
   */
  virtual unsigned CountRows() const = 0;

  /**
   * Tests if a cell is null.
   */
  virtual bool IsNull(unsigned rowIndex, unsigned colIndex) const = 0;

  /**
   * Return a cell value as text.
   */
  virtual wxString AsText(unsigned rowIndex, unsigned colIndex) const = 0;

  /**
   * Return a cell value as UTF-8.
   *
   * This is an optional operation. If NULL is returned (as this base
   * class does), AsText() must be used to get the cell value.
   */
  virtual const char *AsRawText(unsigned rowIndex, unsigned colIndex) const { return NULL; }
};

class QueryResultsAccessor : public ResultGridData {
public:
  QueryResultsAccessor(PGresult *rs) : rs(rs) {}
  QueryResultsAccessor(const QueryResults rs) : rs(rs) {}

  unsigned CountRows() const { return rs.size(); }
  bool IsNull(unsigned rowIndex, unsigned colIndex) const { return false; }
  wxString AsText(unsigned rowIndex, unsigned colIndex) const { return rs[rowIndex][colIndex]; }

private:
  QueryResults rs;
};

class PgResultAccessor : public ResultGridData {
public:
  PgResultAccessor(PGresult *rs) : rs(rs) {}

  unsigned CountRows() const { return PQntuples(rs); }
  bool IsNull(unsigned rowIndex, unsigned colIndex) const { return PQgetisnull(rs, rowIndex, colIndex); }
  wxString AsText(unsigned rowIndex, unsigned colIndex) const { return wxString(PQgetvalue(rs, rowIndex, colIndex), wxConvUTF8); }
  const char* AsRawText(unsigned rowIndex, unsigned colIndex) const { return PQgetvalue(rs, rowIndex, colIndex); }

private:
  PGresult *rs;
};

class ResultChunkAccessor : public ResultGridData {
public:
  ResultChunkAccessor() : rows(0) {}
  ~ResultChunkAccessor() {
    for (std::vector<ResultGridData*>::iterator iter = chunks.begin(); iter != chunks.end(); iter++) {
      delete (*iter);
    }
  }

  void AddChunk(ResultGridData* chunk) { chunks.push_back(chunk); rows += chunk->CountRows(); }

  unsigned CountRows() const { return rows; };

#if 0
  template<typename T, typename F>
  T delegate(unsigned rowIndex, unsigned colIndex) const
  {
    std::vector<ResultGridData*>::const_iterator iter = chunks.begin();
    for (;;) {
      unsigned chunkRows = (*iter)->CountRows();
      if (rowIndex < chunkRows) return (*iter)->IsNull(rowIndex, colIndex);
      rowIndex -= chunkRows;
      if (++iter == chunks.end()) throw std::out_of_range("row number");
    }
  }
#endif

  bool IsNull(unsigned rowIndex, unsigned colIndex) const
  {
    std::vector<ResultGridData*>::const_iterator iter = chunks.begin();
    for (;;) {
      unsigned chunkRows = (*iter)->CountRows();
      if (rowIndex < chunkRows) return (*iter)->IsNull(rowIndex, colIndex);
      rowIndex -= chunkRows;
      if (++iter == chunks.end()) throw std::out_of_range("row number");
    }
  }
  wxString AsText(unsigned rowIndex, unsigned colIndex) const
  {
    std::vector<ResultGridData*>::const_iterator iter = chunks.begin();
    for (;;) {
      unsigned chunkRows = (*iter)->CountRows();
      if (rowIndex < chunkRows) return (*iter)->AsText(rowIndex, colIndex);
      rowIndex -= chunkRows;
      if (++iter == chunks.end()) throw std::out_of_range("row number");
    }
  }
  const char* AsRawText(unsigned rowIndex, unsigned colIndex) const
  {
    std::vector<ResultGridData*>::const_iterator iter = chunks.begin();
    for (;;) {
      unsigned chunkRows = (*iter)->CountRows();
      if (rowIndex < chunkRows) return (*iter)->AsRawText(rowIndex, colIndex);
      rowIndex -= chunkRows;
      if (++iter == chunks.end()) throw std::out_of_range("row number");
    }
  }

private:
  std::vector<ResultGridData*> chunks;
  unsigned rows;
};

class ArrayResultChunkAccessor : public ResultGridData {
public:
  ArrayResultChunkAccessor() : rows(0), used(0) {}
  ~ArrayResultChunkAccessor() {
    for (unsigned i = 0; i < used; i++) {
      delete chunks[i];
    }
  }

  void AddChunk(ResultGridData* chunk)
  {
    if (used >= maxChunks) throw std::out_of_range("row number");
    chunks[used++] = chunk;
    rows += chunk->CountRows();
  }

  unsigned CountRows() const { return rows; };

  bool IsNull(unsigned rowIndex, unsigned colIndex) const
  {
    for (unsigned i = 0; i < used; i++) {
      unsigned chunkRows = chunks[i]->CountRows();
      if (rowIndex < chunkRows) return chunks[i]->IsNull(rowIndex, colIndex);
      rowIndex -= chunkRows;
    }
    throw std::out_of_range("row number");
  }
  wxString AsText(unsigned rowIndex, unsigned colIndex) const
  {
    for (unsigned i = 0; i < used; i++) {
      unsigned chunkRows = chunks[i]->CountRows();
      if (rowIndex < chunkRows) return chunks[i]->AsText(rowIndex, colIndex);
      rowIndex -= chunkRows;
    }
    throw std::out_of_range("row number");
  }
  const char* AsRawText(unsigned rowIndex, unsigned colIndex) const
  {
    for (unsigned i = 0; i < used; i++) {
      unsigned chunkRows = chunks[i]->CountRows();
      if (rowIndex < chunkRows) return chunks[i]->AsRawText(rowIndex, colIndex);
      rowIndex -= chunkRows;
    }
    throw std::out_of_range("row number");
  }

private:
  static const unsigned maxChunks = 16;
  ResultGridData* chunks[maxChunks];
  unsigned rows;
  unsigned used;
};

class CharacterHeapChunkAccessor : public ResultGridData {
public:
  CharacterHeapChunkAccessor(PGresult *rs)
  {
    rowCount = PQntuples(rs);
    columnCount = PQnfields(rs);
    unsigned cellCount = rowCount * columnCount;
    index = new unsigned[rowCount];
    nullmask = new wxUint64[cellCount / 64];
    size_t dataLength = 0;
    for (unsigned row = 0; row < rowCount; row++) {
      for (unsigned column = 0; column < columnCount; column++) {
	if (!PQgetisnull(rs, row, column))
	  dataLength += std::strlen(PQgetvalue(rs, row, column)) + 1;
      }
    }
    heap = new char[dataLength];
    size_t dataPtr = 0;
    unsigned cellIndex = 0;
    for (unsigned row = 0; row < rowCount; row++) {
      index[row] = dataPtr;
      for (unsigned column = 0; column < columnCount; column++) {
	if (PQgetisnull(rs, row, column)) {
	  nullmask[cellIndex / 64] |= (((wxUint64)1) << (cellIndex & 63));
	}
	else {
	  char *value = heap + dataPtr;
	  std::strcpy(value, PQgetvalue(rs, row, column));
	  dataPtr += std::strlen(value);
	}
	cellIndex++;
      }
    }
  }

  unsigned CountRows() const { return rowCount; }
  bool IsNull(unsigned rowIndex, unsigned colIndex) const
  {
    unsigned cellIndex = rowIndex * columnCount + colIndex;
    return nullmask[cellIndex / 64] & (((wxUint64)1) << (cellIndex & 63));
  }
  const char* AsRawText(unsigned rowIndex, unsigned colIndex) const
  {
    unsigned dataPtr = index[rowIndex];
    for (unsigned col = 0; col < colIndex; col++) {
      if (!IsNull(rowIndex, col)) {
	dataPtr += std::strlen(heap + dataPtr) + 1;
      }
    }
    return heap + dataPtr;
  }
  wxString AsText(unsigned rowIndex, unsigned colIndex) const
  {
    return wxString(AsRawText(rowIndex, colIndex), wxConvUTF8);
  }

private:
  unsigned rowCount;
  unsigned columnCount;
  PQWX::auto_array<char> heap;
  PQWX::auto_array<unsigned> index;
  PQWX::auto_array<wxUint64> nullmask;

  // forbid copying
  CharacterHeapChunkAccessor(const CharacterHeapChunkAccessor&);
  CharacterHeapChunkAccessor& operator=(const CharacterHeapChunkAccessor&);
};

class HeapMappedFileAccessor : public ResultGridData {
public:
  HeapMappedFileAccessor(const std::string &filename) : fd(-1), base(NULL)
  {
    struct stat statbuf;
    if (stat(filename.c_str(), &statbuf) < 0) throw std::runtime_error("unable to stat datafile");
    fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("unable to open datafile");
    length = statbuf.st_size;
    base = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) throw std::runtime_error("unable to map datafile");

    if (Magic() != magic) throw std::runtime_error("invalid magic");

    cellCount = CountRows() * ColumnCount();
    nullMask = (wxUint64*) WordPtr(3);
    index = (unsigned*) (nullMask + (cellCount+63)/64);
    heap = (char*) (index + CountRows());
  }
  ~HeapMappedFileAccessor()
  {
    if (base != NULL && base != MAP_FAILED) {
      munmap(base, length);
    }
    if (fd >= 0) {
      posix_fadvise(fd, 0, length, POSIX_FADV_DONTNEED);
      close(fd);
    }
  }

  unsigned CountRows() const { return *WordPtr(1); }
  bool IsNull(unsigned rowIndex, unsigned colIndex) const {
    unsigned cellIndex = colIndex * ColumnCount() + colIndex;
    return nullMask[cellIndex / 64] & (((wxUint64)1) << (cellIndex & 63));
  }
  const char* AsRawText(unsigned rowIndex, unsigned colIndex) const {
    unsigned dataPtr = index[rowIndex];
    for (unsigned col = 0; col < colIndex; col++) {
      if (!IsNull(rowIndex, col)) {
	dataPtr += std::strlen(heap + dataPtr) + 1;
      }
    }
    return heap + dataPtr;
  }
  wxString AsText(unsigned rowIndex, unsigned colIndex) const { return wxString(AsRawText(rowIndex, colIndex), wxConvUTF8); }

private:
  int fd;
  void *base;
  size_t length;
  unsigned cellCount;
  unsigned *index;
  wxUint64 *nullMask;
  char *heap;

  static const unsigned magic = 0x50515758;

  const unsigned* WordPtr(unsigned n) const { return ((unsigned*) base) + n; }
  unsigned Magic() const { return *WordPtr(0); }
  unsigned ColumnCount() const { return *WordPtr(2); }

  // forbid copying
  HeapMappedFileAccessor(const HeapMappedFileAccessor&);
  HeapMappedFileAccessor& operator=(const HeapMappedFileAccessor&);
};

static HeapMappedFileAccessor TEST("/tmp/x");
static CharacterHeapChunkAccessor TEST2(NULL);

#if 0
std::auto_ptr<ResultGridData*> FetchResultGridData(PGconn *conn, const char *sql)
{
  ResultChunkAccessor *chunks = new ResultChunkAccessor();
  std::auto_ptr<ResultGridData*> resultPtr = chunks;

  unsigned maxRows = initialRows;
  PQbeginQuery(conn, sql, 0, NULL, NULL, NULL, maxRows);
  PGresult *rs = PQgetResult(conn);
  if (PQresultStatus(rs) != PGRES_TUPLES_OK) return null;
  chunks->AddChunk(new QueryResultsAccessor(rs));
  while (haveMore) {
    if (maxRows < maxRowsLimit) {
      maxRows = (maxRows << 1) + maxRows;
      if (maxRows > maxRowsLimit)
	maxRows = maxRowsLimit;
    }
    PQcontinueQuery(conn, maxRows);
    rs = PQgetResult(conn);
    while (rs != NULL) {
      chunks->AddChunk(new PgResultAccessor(rs));
      rs = PQgetResult(conn);
    }
  }
  PQfinaliseQuery(conn);

  return resultPtr;
}
#endif

#endif

// Local Variables:
// mode: c++
// End:
