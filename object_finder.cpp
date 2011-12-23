#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <vector>
#include <set>
#include "wx/xrc/xmlres.h"
#include "wx/config.h"
#include "wx/regex.h"
#include "object_finder.h"
#include "object_browser.h"

using namespace std;

BEGIN_EVENT_TABLE(ObjectFinder, wxDialog)
  EVT_TEXT(XRCID("query"), ObjectFinder::OnQueryChanged)
  EVT_BUTTON(wxID_OK, ObjectFinder::OnOk)
  EVT_BUTTON(wxID_CANCEL, ObjectFinder::OnCancel)
  EVT_CLOSE(ObjectFinder::OnClose)
  EVT_LISTBOX_DCLICK(Pqwx_ObjectFinderResults, ObjectFinder::OnDoubleClickResult)
END_EVENT_TABLE()

static wxRegEx schemaPattern(_T("^([a-zA-Z_][a-zA-Z0-9_]*)\\."));

void ObjectFinder::OnQueryChanged(wxCommandEvent &event) {
  wxString query = queryInput->GetValue();

  resultsCtrl->Clear();

  if (!query.IsEmpty()) {
    if (schemaPattern.Matches(query)) {
      wxString schema = schemaPattern.GetMatch(query, 1);
      results = catalogue->Search(query, filter & catalogue->CreateSchemaFilter(schema));
    }
    else {
      results = catalogue->Search(query, filter);
    }
  }
  else {
    results.clear();
  }

  wxArrayString htmlList;
  map<wxString, unsigned> seenSymbols;
  set<wxString> dupeSymbols;

  for (vector<CatalogueIndex::Result>::iterator iter = results.begin(); iter != results.end(); iter++) {
    wxString html;

    const wxString &symbol = iter->document->symbol;
    size_t pos = 0;
    for (vector<CatalogueIndex::Result::Extent>::iterator extentIter = (*iter).extents.begin(); extentIter != (*iter).extents.end(); extentIter++) {
      int skip = (*extentIter).offset - pos;
      if (skip > 0) {
	html << symbol.Mid(pos, skip);
	pos += skip;
      }
      html << _T("<b>");
      html << symbol.Mid(pos, (*extentIter).length);
      pos += (*extentIter).length;
      html << _T("</b>");
    }
    int residual = symbol.length() - pos;
    if (residual > 0) {
      html << symbol.Mid(pos);
    }

    bool firstRepeat = seenSymbols.count(symbol) > 0;
    bool subsequentRepeat = dupeSymbols.count(symbol) > 0;
    if (firstRepeat || subsequentRepeat) {
      if (!iter->document->disambig.IsEmpty()) {
	html << _T('(') << iter->document->disambig << _T(')');
      }
      if (!subsequentRepeat) {
	unsigned index = seenSymbols[symbol];
	const CatalogueIndex::Result &firstResult = results[index];
	htmlList[index] << _T('(') << firstResult.document->disambig << _T(')');
	dupeSymbols.insert(symbol);
      }
    }
    else {
      seenSymbols[symbol] = htmlList.size();
    }

    htmlList.Add(html);
  }

  resultsCtrl->Append(htmlList);
}

void ObjectFinder::OnOk(wxCommandEvent &event) {
  int n = resultsCtrl->GetSelection();
  // should really disable the button when no result is selected
  if (n == wxNOT_FOUND)
    return;
  OnDoubleClickResult(event);
}

void ObjectFinder::OnDoubleClickResult(wxCommandEvent &event) {
  int n = resultsCtrl->GetSelection();
  wxASSERT(n != wxNOT_FOUND);
  wxASSERT(n < results.size());
  const CatalogueIndex::Result &result = results[n];
  wxLogDebug(_T("Open object: %s"), result.document->symbol.c_str());
  completion->OnObjectChosen(result.document);

  Destroy();
}

void ObjectFinder::OnCancel(wxCommandEvent &event) {
  completion->OnCancelled();
  Destroy();
}

void ObjectFinder::OnClose(wxCloseEvent &event) {
  completion->OnCancelled();
  Destroy();
}

void ObjectFinder::Init(wxWindow *parent) {
  wxXmlResource::Get()->LoadDialog(this, parent, _T("ObjectFinder"));
  queryInput = XRCCTRL(*this, "query", wxTextCtrl);
  // bodge-tastic... xrced doesn't support wxSimpleHtmlListBox
  wxListBox *dummyResultsCtrl = XRCCTRL(*this, "results", wxListBox);
  resultsCtrl = new wxSimpleHtmlListBox(this, Pqwx_ObjectFinderResults);
  resultsCtrl->GetFileSystem().ChangePathTo(_T("memory:ObjectFinder"), true);
  GetSizer()->Replace(dummyResultsCtrl, resultsCtrl);
  dummyResultsCtrl->Destroy();
}
