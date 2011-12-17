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
  void Begin() {
#ifdef PQWX_DEBUG
    gettimeofday(&start, NULL);
#endif
  }
  void AddDocument(long entityId, Type entityType, const wxString& symbol, const wxString& disambig = wxEmptyString);
  void Commit() {
#ifdef PQWX_DEBUG
    gettimeofday(&finish, NULL);
    struct timeval elapsed;
    timersub(&finish, &start, &elapsed);
    double elapsedFP = (double) elapsed.tv_sec + ((double) elapsed.tv_usec / 1000000.0);
    wxLogDebug(_T("** Indexed %d terms over %d documents in %.3lf seconds"), terms.size(), documents.size(), elapsedFP);
#endif
  }

  class Document {
  public:
    Document(long entityId, Type entityType, const wxString& symbol, const wxString& disambig) : entityId(entityId), entityType(entityType), symbol(symbol), disambig(disambig) {}
    long entityId;
    Type entityType;
    wxString symbol;
    wxString disambig;
  };

  class Result {
  public:
    Result(Document *document, int score) : document(document), score(score) {}
    Document *document;
    int score;
  };

  void Search(const wxString &input);

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

  std::vector<wxString> Analyse(const wxString &input);
  std::vector<Document> documents;
  std::vector<wxString> terms;
  std::map<wxString, int> termsIndex;
  std::map<wxString, std::vector<int> > prefixes;
  std::map<int, std::vector<int> > documentTerms;
  std::map<int, std::vector<Occurrence> > occurrences;
  std::vector<int> MatchTerms(const wxString &token);
};

#endif
