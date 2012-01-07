/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __catalogue_index_h
#define __catalogue_index_h

#include <vector>
#include <map>
#include <iterator>
#include "wx/string.h"
#include "wx/log.h"
#include "wx/stopwatch.h"

/**
 * Database catalogue search index.
 *
 * Stores a set of "documents", each representing a named database
 * object. The names are analysed by splitting up words separated by
 * underscores, or described by StudlyCaps. Searches can then be made
 * using a phrase query analysed in the same way, and optionally
 * filtered by what database object type each document has, and
 * whether it is considered a "system object" or not.
 *
 * The usage scheme is:
 * <pre>
 *     CatalogueIndex index;
 *     index.Begin();
 *     index.AddDocument(Document(oid, type, system, symbol));
 *     // etc...
 *     index.Commit();
 *     // to search:
 *     CatalogueIndex::Filter filter = index.CreateNonSystemFiltber();
 *     std::vector<CatalogueIndex::Result> = index.Search("MyTab", filter);
 * </pre>
 */
class CatalogueIndex {
public:
  /**
   * Types of database object.
   *
   * Somewhat, but not exactly, equivalent to pg_class. For example,
   * there are multiple types here for functions (pg_proc) so that
   * callers can individually filter out trigger functions, for
   * example.
   */
  enum Type { TABLE, VIEW, SEQUENCE,
	      FUNCTION_SCALAR, FUNCTION_ROWSET, FUNCTION_TRIGGER, FUNCTION_AGGREGATE, FUNCTION_WINDOW,
	      TYPE, EXTENSION, COLLATION };

  /**
   * Datum stored in the search index.
   *
   * These are the potential results searched by the query.
   */
  class Document {
  public:
    Document(long entityId, Type entityType, bool system, const wxString& symbol, const wxString& disambig = wxEmptyString) : entityId(entityId), entityType(entityType), symbol(symbol), disambig(disambig), system(system) {}
    long entityId;
    Type entityType;
    wxString symbol;
    wxString disambig;
    bool system;
  };

  /**
   * Begin indexing.
   *
   * Currently does nothing except setup the timer for logging how long indexing takes.
   */
  void Begin() {
#ifdef __WXDEBUG__
    stopwatch.Start();
#endif
  }
  /**
   * Adds a document to the search index.
   */
  void AddDocument(const Document& document);
  /**
   * Finish indexing.
   *
   * Currently does nothing except log how long indexing took, in debug mode.
   */
  void Commit() {
#ifdef __WXDEBUG__
    wxLogDebug(_T("** Indexed %lu terms over %lu documents in %.3lf seconds"), terms.size(), documents.size(), stopwatch.Time() / 1000.0);
#endif
  }

  /**
   * Catalogue search result.
   *
   * A search result consists of:
   * <ul>
   *  <li> A document that was matched. </li>
   *  <li> A vector of extents, indicating which parts of the input query were used to make the match. </li>
   *  <li> A score that indicates how well this document matched the query- a higher score is a "better" match. </li>
   * </ul>
   */
  class Result {
  public:
    /**
     * Describes a part of the input query string was matched
     */
    class Extent {
    public:
      Extent(size_t offset, size_t length) : offset(offset), length(length) {}
      size_t offset;
      size_t length;
    };
    Result(const Document *document, int score, const std::vector<Extent> &extents) : document(document), score(score), extents(extents) {}
    const Document *document;
    int score;
    std::vector<Extent> extents;
    bool operator<(const CatalogueIndex::Result &r2) const {
      return score < r2.score
	|| (score == r2.score && document->symbol < r2.document->symbol);
    }
  };

  /**
   * Catalogue search filter.
   *
   * A filter is a mask that can accept or reject potential search
   * matches. So after a match candidate is found, the filter is
   * called to test if it should be returned to the caller.
   *
   * Filters are implemented as multi-word bitmasks, and implement
   * most of the bitmask-manipulation operators.
   *
   * A filter is created with a fixed capacity, usually based on a
   * particular index using the CatalogueIndex factory methods. Trying
   * to operate on filters with different capacities is an error. In
   * general, only filters that were based on the same index are
   * useful to combine together.
   */
  class Filter {
  public:
    Filter(int capacity) : capacity(capacity) {
      data = new wxUint64[NumWords()];
      for (int i = NumWords() - 1; i >= 0; i--) {
	data[i] = 0;
      }
    }
    Filter(const Filter &other) : capacity(other.capacity) {
      data = new wxUint64[NumWords()];
      for (int i = NumWords() - 1; i >= 0; i--) {
	data[i] = other.data[i];
      }
    }
    virtual ~Filter() {
      delete[] data;
    }
    void Include(int pos) {
      wxASSERT(pos < capacity);
      data[pos >> 6] |= ((wxUint64) 1)<<(pos & 63);
    }
    bool Included(int pos) const {
      wxASSERT(pos < capacity);
      return data[pos >> 6] & (((wxUint64) 1)<<(pos & 63));
    }
    void Clear() {
      for (int i = NumWords() - 1; i >= 0; i--) {
	data[i] = 0;
      }
    }
    void operator =(const Filter &other) {
      wxASSERT(other.capacity == capacity);
      for (int i = NumWords() - 1; i >= 0; i--) {
	data[i] = other.data[i];
      }
    }
    void operator &=(const Filter &other) {
      wxASSERT(other.capacity == capacity);
      for (int i = NumWords() - 1; i >= 0; i--) {
	data[i] &= other.data[i];
      }
    }
    void operator |=(const Filter &other) {
      wxASSERT(other.capacity == capacity);
      for (int i = NumWords() - 1; i >= 0; i--) {
	data[i] |= other.data[i];
      }
    }
    Filter operator&(const Filter &other) const {
      wxASSERT(other.capacity == capacity);
      Filter result(capacity);
      for (int i = result.NumWords() - 1; i >= 0; i--) {
	result.data[i] = data[i] & other.data[i];
      }
      return result;
    }
    Filter operator|(const Filter &other) const {
      wxASSERT(other.capacity == capacity);
      Filter result(capacity);
      for (int i = result.NumWords() - 1; i >= 0; i--) {
	result.data[i] = data[i] | other.data[i];
      }
      return result;
    }
    Filter operator~() const {
      Filter result(capacity);
      for (int i = result.NumWords() - 1; i >= 0; i--) {
	result.data[i] = ~data[i];
      }
      return result;
    }
    int cardinality() const {
      int result = 0;
      for (int i = NumWords() - 1; i >= 0; i--) {
	wxUint64 mask = 1;
	for (int j = 0; j < 64; j++) {
	  if (data[i] & mask) result++;
	  mask = mask << 1;
	}
      }
      return result;
    }
    bool IsEmpty() const {
      for (int i = NumWords() - 1; i >= 0; i--) {
	if (data[i]) return false;
      }
      return true;
    }
  private:
    int NumWords() const {
      return (capacity+63) >> 6;
    }
    int capacity;
    wxUint64 *data;
    friend class CatalogueIndex;
  };

  /**
   * Creates a filter that matches every document in the index.
   */
  Filter CreateMatchEverythingFilter() const {
    Filter filter(documents.size());
    for (int i = filter.NumWords() - 1; i >= 0; i--) {
      filter.data[i] = (wxUint64) -1;
    }
    return filter;
  };
  /**
   * Creates a filter that only matches documents that are not flagged as representing "system" objects.
   */
  Filter CreateNonSystemFilter() const;
  /**
   * Creates a filter that only matches documents representing the specified type.
   */
  Filter CreateTypeFilter(Type type) const;
  /**
   * Creates a filter that only matches documents representing objects in the specified schema.
   */
  Filter CreateSchemaFilter(const wxString &schema) const;

  /**
   * Search the catalogue index, specifying a query, filter and maximum number of results.
   *
   * Results are returned in descending score order.
   */
  std::vector<Result> Search(const wxString &input, const Filter &filter, unsigned maxResults = 100) const;

#ifdef PQWX_DEBUG_CATALOGUE_INDEX
  void DumpDocumentStore() {
    int documentId = 0;
    for (std::vector<Document>::iterator docIter = documents.begin(); docIter != documents.end(); docIter++, documentId++) {
      wxString documentDump;
      for (std::vector<int>::iterator termIter = documentTerms[documentId].begin(); termIter != documentTerms[documentId].end(); termIter++) {
	documentDump << _T(" | ") << *termIter << _T(":\"") << terms[*termIter] << _T("\"");
      }
      wxLogDebug(_T("Document#%d : %s"), documentId, documentDump.Mid(3).c_str());
    }
  }
#endif

  static wxString EntityTypeName(Type type);

private:
#ifdef __WXDEBUG__
  wxStopWatch stopwatch;
#endif
  class DocumentPosition {
  public:
    DocumentPosition(int documentId, int position) : documentId(documentId), position(position) {}
    int documentId;
    int position;
    bool operator< (const DocumentPosition &other) const {
      return documentId < other.documentId
	|| (documentId == other.documentId && position < other.position);
    }
  };
  class Occurrence : public DocumentPosition {
  public:
    Occurrence(int documentId, int termId, int position, size_t offset) : DocumentPosition(documentId, position), offset(offset), termId(termId) {}
    size_t offset;
    int termId;
  };

  const std::vector<Occurrence> * TermOccurrences(int termId) const {
    std::map<int, std::vector<Occurrence> >::const_iterator iter = occurrences.find(termId);
    wxASSERT(iter != occurrences.end());
    return &( iter->second );
  }

  const std::vector<int> * DocumentTerms(int documentId) const {
    std::map<int, std::vector<int> >::const_iterator iter = documentTerms.find(documentId);
    wxASSERT(iter != documentTerms.end());
    return &( iter->second );
  }

  class Token {
  public:
    Token(const wxString &value, size_t inputPosition) : value(value), inputPosition(inputPosition) {}
    wxString value;
    size_t inputPosition;
  };

  std::vector<Token> Analyse(const wxString &input) const;
  std::vector<Document> documents;
  std::vector<wxString> terms;
  std::map<wxString, int> termsIndex;
  std::map<wxString, std::vector<int> > prefixes;
  std::map<int, std::vector<int> > documentTerms;
  std::map<int, std::vector<Occurrence> > occurrences;
  std::vector<int> MatchTerms(const wxString &token) const;
};

#endif

// Local Variables:
// mode: c++
// End:
