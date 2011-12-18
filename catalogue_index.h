// -*- mode: c++ -*-

#ifndef __catalogue_index_h
#define __catalogue_index_h

#include <vector>
#include <map>
#include <iterator>
#include <sys/time.h>
#include "wx/string.h"
#include "wx/log.h"

class CatalogueIndex {
public:
  enum Type { TABLE, VIEW, SEQUENCE,
	      FUNCTION_SCALAR, FUNCTION_ROWSET, FUNCTION_TRIGGER, FUNCTION_AGGREGATE, FUNCTION_WINDOW,
	      TYPE, EXTENSION, COLLATION };

  class Document {
  public:
    Document(long entityId, Type entityType, bool system, const wxString& symbol, const wxString& disambig = wxEmptyString) : entityId(entityId), entityType(entityType), system(system), symbol(symbol), disambig(disambig) {}
    long entityId;
    Type entityType;
    wxString symbol;
    wxString disambig;
    bool system;
  };

  void Begin() {
#ifdef PQWX_DEBUG
    gettimeofday(&start, NULL);
#endif
  }
  void AddDocument(const Document& document);
  void Commit() {
#ifdef PQWX_DEBUG
    gettimeofday(&finish, NULL);
    struct timeval elapsed;
    timersub(&finish, &start, &elapsed);
    double elapsedFP = (double) elapsed.tv_sec + ((double) elapsed.tv_usec / 1000000.0);
    wxLogDebug(_T("** Indexed %d terms over %d documents in %.3lf seconds"), terms.size(), documents.size(), elapsedFP);
#endif
  }

  class Result {
  public:
    Result(const Document *document, int score) : document(document), score(score) {}
    const Document *document;
    int score;
  };

  // assumption that "unsigned long" is 64 bits ahoy!
  class Filter {
  public:
    Filter(int capacity) : capacity(capacity) {
      data = new unsigned long[NumWords()];
      for (int i = NumWords() - 1; i >= 0; i--) {
	data[i] = 0UL;
      }
    }
    Filter(const Filter &other) : capacity(other.capacity) {
      data = new unsigned long[NumWords()];
      for (int i = NumWords() - 1; i >= 0; i--) {
	data[i] = other.data[i];
      }
    }
    virtual ~Filter() {
      delete[] data;
    }
    void Include(int pos) {
      wxASSERT(pos < capacity);
      data[pos >> 6] |= 1UL<<(pos & 63);
    }
    bool Included(int pos) const {
      wxASSERT(pos < capacity);
      return data[pos >> 6] & (1UL<<(pos & 63));
    }
    void Clear() {
      for (int i = NumWords() - 1; i >= 0; i--) {
	data[i] = 0UL;
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
  private:
    int NumWords() const {
      return (capacity+63) >> 6;
    }
    int capacity;
    unsigned long *data;
    friend class CatalogueIndex;
  };

  Filter CreateMatchEverythingFilter() const {
    Filter filter(documents.size());
    for (int i = filter.NumWords() - 1; i >= 0; i--) {
      filter.data[i] = 0xffffffffffffffffUL;
    }
    return filter;
  };
  Filter CreateNonSystemFilter() const;
  Filter CreateTypeFilter(Type type) const;
  Filter CreateSchemaFilter(const wxString &schema) const;

  std::vector<Result> Search(const wxString &input, const Filter &filter) const;

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
#ifdef PQWX_DEBUG
  struct timeval start, finish;
#endif
  class DocumentPosition {
  public:
    DocumentPosition(int documentId, int position) : documentId(documentId), position(position) {}
    int documentId;
    int position;
    bool operator< (const DocumentPosition &other) const {
      return documentId < other.documentId
	|| documentId == other.documentId && position < other.position;
    }
  };
  class Occurrence : public DocumentPosition {
  public:
    Occurrence(int documentId, int termId, int position) : DocumentPosition(documentId, position), termId(termId) {}
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

  std::vector<wxString> Analyse(const wxString &input) const;
  std::vector<Document> documents;
  std::vector<wxString> terms;
  std::map<wxString, int> termsIndex;
  std::map<wxString, std::vector<int> > prefixes;
  std::map<int, std::vector<int> > documentTerms;
  std::map<int, std::vector<Occurrence> > occurrences;
  std::vector<int> MatchTerms(const wxString &token) const;
};

#endif
