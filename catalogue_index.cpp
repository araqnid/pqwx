#include "catalogue_index.h"
#include "wx/log.h"
#include <algorithm>
#include <queue>

using namespace std;

void CatalogueIndex::AddDocument(const Document& document) {
  int documentId = documents.size();
  documents.push_back(document);
  vector<Token> tokens(Analyse(document.symbol));
  int pos = 0;
  for (vector<Token>::iterator iter = tokens.begin(); iter != tokens.end(); iter++, pos++) {
    map<wxString, int>::iterator termIter = termsIndex.find((*iter).value);
    int termId;
    if (termIter == termsIndex.end()) {
      termId = terms.size();
      terms.push_back((*iter).value);
      termsIndex[(*iter).value] = termId;
      // still doing this the perl-ish way
      for (unsigned len = 1; len <= (*iter).value.length(); len++) {
	prefixes[(*iter).value.Left(len)].push_back(termId);
      }
    }
    else {
      termId = termIter->second;
    }
    documentTerms[documentId].push_back(termId);
    occurrences[termId].push_back(Occurrence(documentId, termId, pos, (*iter).inputPosition));
  }
}

vector<CatalogueIndex::Token> CatalogueIndex::Analyse(const wxString &input) const {
  vector<CatalogueIndex::Token> output;

  int mark = -1; // start of current token
  // [mark,pos) is the token when we find an edge
  for (unsigned pos = 0; pos < input.length(); pos++) {
    wxChar c = input[pos];
    if (iswalnum(c)) {
      if (mark < 0) {
	mark = pos;
      }
      else if (iswupper(c)) {
	// upper-case letter causes a flush
	output.push_back(Token(input.Mid(mark, pos-mark), (size_t) mark));
	mark = pos;
      }
    }
    else if (mark >= 0) {
      // moved from alphanumeric to other
      output.push_back(Token(input.Mid(mark, pos-mark), (size_t) mark));
      mark = -1;
    }
  }

  if (mark >= 0)
    output.push_back(Token(input.Mid(mark), (size_t) mark)); // moved to end-of-string

  for (vector<Token>::iterator iter = output.begin(); iter != output.end(); iter++) {
    (*iter).value.MakeLower();
  }

  return output;
}

vector<int> CatalogueIndex::MatchTerms(const wxString &token) const {
  map<wxString, vector<int> >::const_iterator iter = prefixes.find(token);
  if (iter == prefixes.end()) {
    vector<int> empty;
    return empty;
  }
  return iter->second;
}

static const wxString TYPE_NAMES[] = {
  _T("table"), _T("view"), _T("sequence"),
  _T("function"), _T("rowset-function"), _T("trigger-function"), _T("aggregate-function"), _T("window-function"),
  _T("type"), _T("extension"), _T("collation")
};

wxString CatalogueIndex::EntityTypeName(Type type) {
  return TYPE_NAMES[type];
}

vector<CatalogueIndex::Result> CatalogueIndex::Search(const wxString &input, const Filter &filter, unsigned maxResults) const {
#ifdef PQWX_DEBUG
  struct timeval start;
  gettimeofday(&start, NULL);
#endif
  if (filter.IsEmpty()) return vector<CatalogueIndex::Result>();
  vector<Token> tokens = Analyse(input);
  if (tokens.empty()) return vector<CatalogueIndex::Result>();
  vector< map<const DocumentPosition, const Occurrence*> > tokenMatches;
  for (vector<Token>::iterator iter = tokens.begin(); iter != tokens.end(); iter++) {
    map<const DocumentPosition, const Occurrence*> tokenOccurrences;
    vector<int> matchedTerms = MatchTerms((*iter).value);
    for (vector<int>::iterator matchedTermsIter = matchedTerms.begin(); matchedTermsIter != matchedTerms.end(); matchedTermsIter++) {
      const vector<Occurrence> * termOccurrences = TermOccurrences(*matchedTermsIter);
      for (vector<Occurrence>::const_iterator occurrenceIter = termOccurrences->begin(); occurrenceIter != termOccurrences->end(); occurrenceIter++) {
	tokenOccurrences[*occurrenceIter] = &(*occurrenceIter);
      }
    }
    tokenMatches.push_back(tokenOccurrences);
  }
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
  int dumpTokenPosition = 0;
  for (vector< map<DocumentPosition, Occurrence*> >::iterator iter = tokenMatches.begin(); iter != tokenMatches.end(); iter++, dumpTokenPosition++) {
    wxLogDebug(_T("Token %d=\"%s\":"), dumpTokenPosition, tokens[dumpTokenPosition].c_str());
    for (map<DocumentPosition, Occurrence*>::iterator iter2 = iter->begin(); iter2 != iter->end(); iter2++) {
      wxLogDebug(_T(" Document#%d at %d => Document#%d at %d %d:\"%s\""), iter2->first.documentId, iter2->first.position,
		 iter2->second->documentId, iter2->second->position, iter2->second->termId,
		 terms[iter2->second->termId].c_str());
    }
  }
#endif
  priority_queue<Result> scoreDocs;
  int hitCount = 0;
  for (map<const DocumentPosition, const Occurrence*>::iterator iter = tokenMatches[0].begin(); iter != tokenMatches[0].end(); iter++) {
    const Occurrence *firstTerm = iter->second;
    int documentId = firstTerm->documentId;
    if (!filter.Included(documentId)) {
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
      wxLogDebug(_T("Document#%d excluded by filter"), firstTerm->documentId);
#endif
      continue;
    }
    int position = firstTerm->position;
    vector<const Occurrence*> matched;
    matched.push_back(firstTerm);
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
    wxLogDebug(_T("Matched first term %d:\"%s\" in Document#%d at %d"), firstTerm->termId, terms[firstTerm->termId].c_str(), firstTerm->documentId, firstTerm->position);
#endif
    for (unsigned offset = 1; offset < tokenMatches.size(); offset++) {
      const Occurrence *occurrence = tokenMatches[offset][DocumentPosition(documentId, position + offset)];
      if (!occurrence) {
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
	wxLogDebug(_T(" No match for subsequent token %d=\"%s\" in Document#%d at %d"), offset, tokens[offset].c_str(), documentId, position + offset);
#endif
	break;
      }
      matched.push_back(occurrence);
    }
    if (matched.size() < tokens.size())
      continue;
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
    wxString matchDump;
    for (vector<Occurrence*>::iterator iter = matched.begin(); iter != matched.end(); iter++) {
      matchDump << _T(" | ") << terms[(*iter)->termId];
    }
    wxString queryDump;
    for (vector<wxString>::iterator iter = tokens.begin(); iter != tokens.end(); iter++) {
      queryDump << _T(" | ") << *iter;
    }
    wxString documentDump;
    for (vector<int>::iterator iter = documentTerms[documentId].begin(); iter != documentTerms[documentId].end(); iter++) {
      documentDump << _T(" | ") << *iter;
    }
    wxLogDebug(_T("%s    matched: \"%s\" against \"%s\""), documents[documentId].symbol.c_str(), matchDump.Mid(3).c_str(), queryDump.Mid(3).c_str());
    wxLogDebug(_T(" first position: %d"), firstTerm->position);
    wxLogDebug(_T(" document terms are: %s"), documentDump.Mid(3).c_str());
#endif
    int suffixLength = DocumentTerms(documentId)->size() - matched.size() - firstTerm->position;
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
    wxLogDebug(_T(" trailing terms not matched: %d"), suffixLength);
#endif
    int lastLengthDifference;
    int totalLengthDifference = 0;
    vector<const Occurrence*>::iterator matchIter = matched.begin();
    vector<Token>::iterator tokenIter = tokens.begin();
    vector<Result::Extent> extents;
    for (; matchIter != matched.end(); matchIter++, tokenIter++) {
      wxString termToken = terms[(*matchIter)->termId];
      lastLengthDifference = termToken.length() - (*tokenIter).value.length();
      totalLengthDifference += lastLengthDifference;
      extents.push_back(Result::Extent((*matchIter)->offset, (*tokenIter).value.length()));
    }
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
    wxLogDebug(_T(" last token length difference: %d"), lastLengthDifference);
    wxLogDebug(_T(" other tokens length difference: %d"), totalLengthDifference - lastLengthDifference);
#endif
    // TODO weightings for these
    int score = firstTerm->position + suffixLength + lastLengthDifference + (totalLengthDifference - lastLengthDifference);
    ++hitCount;
    scoreDocs.push(Result(&(documents[documentId]), score, extents));
    if (scoreDocs.size() > maxResults)
      scoreDocs.pop();
  }
#ifdef PQWX_DEBUG
  struct timeval finish;
  gettimeofday(&finish, NULL);
  struct timeval elapsed;
  timersub(&finish, &start, &elapsed);
  double elapsedFP = (double) elapsed.tv_sec + ((double) elapsed.tv_usec / 1000000.0);
  wxLogDebug(_T("** Completed search in %.3lf seconds, and produced %d/%d results"), elapsedFP, scoreDocs.size(), hitCount);
#endif
  vector<Result> resultVector;
  resultVector.reserve(scoreDocs.size());
  while (!scoreDocs.empty()) {
    resultVector.push_back(scoreDocs.top());
    scoreDocs.pop();
  }
  reverse(resultVector.begin(), resultVector.end());
  return resultVector;
}

CatalogueIndex::Filter CatalogueIndex::CreateNonSystemFilter() const {
  Filter filter(documents.size());

  int documentId = 0;
  for (vector<Document>::const_iterator iter = documents.begin(); iter != documents.end(); iter++, documentId++) {
    if (!iter->system) filter.Include(documentId);
  }

  return filter;
}

CatalogueIndex::Filter CatalogueIndex::CreateTypeFilter(CatalogueIndex::Type type) const {
  Filter filter(documents.size());

  int documentId = 0;
  for (vector<Document>::const_iterator iter = documents.begin(); iter != documents.end(); iter++, documentId++) {
    if (iter->entityType == type) filter.Include(documentId);
  }

  return filter;
}

CatalogueIndex::Filter CatalogueIndex::CreateSchemaFilter(const wxString &schema) const {
  Filter filter(documents.size());
  wxString prefix = schema + _T(".");

  int documentId = 0;
  for (vector<Document>::const_iterator iter = documents.begin(); iter != documents.end(); iter++, documentId++) {
    if (iter->symbol.StartsWith(prefix)) filter.Include(documentId);
  }

  return filter;
}
