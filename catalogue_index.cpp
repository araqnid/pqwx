#include "catalogue_index.h"
#include "wx/log.h"
#include <algorithm>

using namespace std;

void CatalogueIndex::AddDocument(const Document& document) {
  int documentId = documents.size();
  documents.push_back(document);
  vector<wxString> tokens(Analyse(document.symbol));
  int pos = 0;
  for (vector<wxString>::iterator iter = tokens.begin(); iter != tokens.end(); iter++, pos++) {
    map<wxString, int>::iterator termIter = termsIndex.find(*iter);
    int termId;
    if (termIter == termsIndex.end()) {
      termId = terms.size();
      terms.push_back(*iter);
      termsIndex[*iter] = termId;
      // still doing this the perl-ish way
      for (int len = 1; len <= iter->length(); len++) {
	prefixes[iter->Left(len)].push_back(termId);
      }
    }
    else {
      termId = termIter->second;
    }
    documentTerms[documentId].push_back(termId);
    occurrences[termId].push_back(Occurrence(documentId, termId, pos));
  }
}

vector<wxString> CatalogueIndex::Analyse(const wxString &input) {
  vector<wxString> output;

  int mark = -1; // start of current token
  // [mark,pos) is the token when we find an edge
  for (int pos = 0; pos < input.length(); pos++) {
    wxChar c = input[pos];
    if (iswalnum(c)) {
      if (mark < 0) {
	mark = pos;
      }
      else if (iswupper(c)) {
	// upper-case letter causes a flush
	output.push_back(input.Mid(mark, pos-mark));
	mark = pos;
      }
    }
    else if (mark >= 0) {
      // moved from alphanumeric to other
      output.push_back(input.Mid(mark, pos-mark));
      mark = -1;
    }
  }

  if (mark >= 0)
    output.push_back(input.Mid(mark)); // moved to end-of-string

  for (vector<wxString>::iterator iter = output.begin(); iter != output.end(); iter++) {
    iter->MakeLower();
  }

  return output;
}

static bool collateResults(const CatalogueIndex::Result &r1, const CatalogueIndex::Result &r2) {
  return r1.score < r2.score
    || (r1.score == r2.score && r1.document->symbol < r2.document->symbol);
}

vector<int> CatalogueIndex::MatchTerms(const wxString &token) {
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

vector<CatalogueIndex::Result> CatalogueIndex::Search(const wxString &input, const Filter &filter) {
#ifdef PQWX_DEBUG
  struct timeval start;
  gettimeofday(&start, NULL);
#endif
  vector<wxString> tokens = Analyse(input);
  vector< map<DocumentPosition, Occurrence*> > tokenMatches;
  for (vector<wxString>::iterator iter = tokens.begin(); iter != tokens.end(); iter++) {
    map<DocumentPosition, Occurrence*> tokenOccurrences;
    vector<int> matchedTerms = MatchTerms(*iter);
    for (vector<int>::iterator matchedTermsIter = matchedTerms.begin(); matchedTermsIter != matchedTerms.end(); matchedTermsIter++) {
      vector<Occurrence> *termOccurrences = &(occurrences[*matchedTermsIter]);
      for (vector<Occurrence>::iterator occurrenceIter = termOccurrences->begin(); occurrenceIter != termOccurrences->end(); occurrenceIter++) {
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
  vector<Result> scoreDocs;
  for (map<DocumentPosition, Occurrence*>::iterator iter = tokenMatches[0].begin(); iter != tokenMatches[0].end(); iter++) {
    Occurrence *firstTerm = iter->second;
    int documentId = firstTerm->documentId;
    if (!filter.Included(documentId)) {
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
      wxLogDebug(_T("Document#%d excluded by filter"), firstTerm->documentId);
#endif
      continue;
    }
    int position = firstTerm->position;
    vector<Occurrence*> matched;
    matched.push_back(firstTerm);
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
    wxLogDebug(_T("Matched first term %d:\"%s\" in Document#%d at %d"), firstTerm->termId, terms[firstTerm->termId].c_str(), firstTerm->documentId, firstTerm->position);
#endif
    for (int offset = 1; offset < tokenMatches.size(); offset++) {
      Occurrence *occurrence = tokenMatches[offset][DocumentPosition(documentId, position + offset)];
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
    int suffixLength = documentTerms[documentId].size() - matched.size() - firstTerm->position;
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
    wxLogDebug(_T(" trailing terms not matched: %d"), suffixLength);
#endif
    int lastLengthDifference;
    int totalLengthDifference = 0;
    vector<Occurrence*>::iterator matchIter = matched.begin();
    vector<wxString>::iterator tokenIter = tokens.begin();
    for (; matchIter != matched.end(); matchIter++, tokenIter++) {
      wxString termToken = terms[(*matchIter)->termId];
      wxString queryToken = *tokenIter;
      lastLengthDifference = termToken.length() - queryToken.length();
      totalLengthDifference += lastLengthDifference;
    }
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
    wxLogDebug(_T(" last token length difference: %d"), lastLengthDifference);
    wxLogDebug(_T(" other tokens length difference: %d"), totalLengthDifference - lastLengthDifference);
#endif
    // TODO weightings for these
    int score = firstTerm->position + suffixLength + lastLengthDifference + (totalLengthDifference - lastLengthDifference);
    scoreDocs.push_back(Result(&(documents[documentId]), score));
  }
  sort(scoreDocs.begin(), scoreDocs.end(), collateResults);
#ifdef PQWX_DEBUG
  struct timeval finish;
  gettimeofday(&finish, NULL);
  for (vector<Result>::iterator iter = scoreDocs.begin(); iter != scoreDocs.end(); iter++) {
    wxString resultDump = iter->document->symbol;
    if (!iter->document->disambig.IsEmpty()) {
      resultDump << _T("(") << iter->document->disambig << _T(")");
    }
    resultDump << _T(' ') << EntityTypeName(iter->document->entityType) << _T('#') << iter->document->entityId;
    resultDump << _T(" (") << iter->score << _T(")");
    wxLogDebug(_T("%s"), resultDump.c_str());
  }
  struct timeval elapsed;
  timersub(&finish, &start, &elapsed);
  double elapsedFP = (double) elapsed.tv_sec + ((double) elapsed.tv_usec / 1000000.0);
  wxLogDebug(_T("** Completed search in %.3lf seconds"), elapsedFP);
#endif
  return scoreDocs;
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
