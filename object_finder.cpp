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
#include "pqwx.h"
#include "object_finder.h"
#include "object_browser.h"

BEGIN_EVENT_TABLE(ObjectFinder, wxDialog)
  EVT_TEXT(XRCID("query"), ObjectFinder::OnQueryChanged)
  EVT_BUTTON(wxID_OK, ObjectFinder::OnOk)
  EVT_BUTTON(wxID_CANCEL, ObjectFinder::OnCancel)
  EVT_CLOSE(ObjectFinder::OnClose)
  EVT_LISTBOX_DCLICK(Pqwx_ObjectFinderResults, ObjectFinder::OnDoubleClickResult)
  EVT_CHECKBOX(XRCID("includeSystem"), ObjectFinder::OnIncludeSystem)
END_EVENT_TABLE()

static wxRegEx schemaPattern(_T("^([a-zA-Z_][a-zA-Z0-9_]*)\\."));

void ObjectFinder::SearchCatalogue()
{
  wxString query = queryInput->GetValue();
  bool includeSystem = includeSystemInput->GetValue();

  resultsCtrl->Clear();

  CatalogueIndex::Filter filter(typesFilter);

  if (!includeSystem) filter &= nonSystemFilter;

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

  if (results.empty())
    return;

  wxArrayString htmlList;
  std::map<wxString, unsigned> seenSymbols;
  std::set<wxString> dupeSymbols;

  for (std::vector<CatalogueIndex::Result>::iterator iter = results.begin(); iter != results.end(); iter++) {
    wxString html;

    wxString icon = findIcon(iter->document->entityType);
    if (!icon.IsEmpty())
      html << _T("<img src='") << icon << _T("'>&nbsp;");

    const wxString &symbol = iter->document->symbol;
    size_t pos = 0;
    for (std::vector<CatalogueIndex::Result::Extent>::iterator extentIter = (*iter).extents.begin(); extentIter != (*iter).extents.end(); extentIter++) {
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
  resultsCtrl->SetSelection(0);
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
  wxASSERT(((unsigned) n) < results.size());
  const CatalogueIndex::Result &result = results[n];
  wxLogDebug(_T("Open object: %s"), result.document->symbol.c_str());
  if (completion != NULL) {
    completion->OnObjectChosen(result.document);
    delete completion;
  }
  else {
    EndModal(result.document->entityId);
  }
  Destroy();
}

void ObjectFinder::OnCancel(wxCommandEvent &event) {
  if (completion != NULL) {
    completion->OnCancelled();
    delete completion;
  }
  else {
    EndModal(0);
  }
  Destroy();
}

void ObjectFinder::OnClose(wxCloseEvent &event) {
  if (completion != NULL) {
    completion->OnCancelled();
    delete completion;
  }
  else {
    EndModal(0);
  }
  Destroy();
}

BEGIN_EVENT_TABLE(ObjectFinder::TextQueryControl, wxTextCtrl)
  EVT_CHAR(TextQueryControl::OnCharacterInput)
END_EVENT_TABLE()

void ObjectFinder::TextQueryControl::OnCharacterInput(wxKeyEvent& event)
{
  if (event.GetKeyCode() == WXK_UP) {
    wxLogDebug(_T("up!"));
    return;
  }
  if (event.GetKeyCode() == WXK_DOWN) {
    wxLogDebug(_T("down!"));
    return;
  }
  event.Skip();
}

void ObjectFinder::Init(wxWindow *parent) {
  wxXmlResource::Get()->LoadDialog(this, parent, _T("ObjectFinder"));
  wxTextCtrl *dummyTextCtrl = XRCCTRL(*this, "query", TextQueryControl);
  queryInput = new TextQueryControl(this, XRCID("query"));
  GetSizer()->Replace(dummyTextCtrl, queryInput);
  queryInput->MoveBeforeInTabOrder(dummyTextCtrl);
  dummyTextCtrl->Destroy();
  includeSystemInput = XRCCTRL(*this, "includeSystem", wxCheckBox);
  // bodge-tastic... xrced doesn't support wxSimpleHtmlListBox
  wxListBox *dummyResultsCtrl = XRCCTRL(*this, "results", wxListBox);
  resultsCtrl = new wxSimpleHtmlListBox(this, Pqwx_ObjectFinderResults);
  resultsCtrl->GetFileSystem().ChangePathTo(_T("memory:ObjectFinder"), true);
  GetSizer()->Replace(dummyResultsCtrl, resultsCtrl);
  resultsCtrl->MoveBeforeInTabOrder(dummyResultsCtrl);
  dummyResultsCtrl->Destroy();

  iconMap[CatalogueIndex::TABLE] = _T("icon_table.png");
  iconMap[CatalogueIndex::TABLE_UNLOGGED] = _T("icon_unlogged_table.png");
  iconMap[CatalogueIndex::VIEW] = _T("icon_view.png");
  iconMap[CatalogueIndex::SEQUENCE] = _T("icon_sequence.png");
  iconMap[CatalogueIndex::FUNCTION_SCALAR] = _T("icon_function.png");
  iconMap[CatalogueIndex::FUNCTION_ROWSET] = _T("icon_function.png");
  iconMap[CatalogueIndex::FUNCTION_AGGREGATE] = _T("icon_function_aggregate.png");
  iconMap[CatalogueIndex::FUNCTION_WINDOW] = _T("icon_function_window.png");
  iconMap[CatalogueIndex::TEXT_CONFIGURATION] = _T("icon_text_search_configuration.png");
  iconMap[CatalogueIndex::TEXT_DICTIONARY] = _T("icon_text_search_dictionary.png");
  iconMap[CatalogueIndex::TEXT_PARSER] = _T("icon_text_search_parser.png");
  iconMap[CatalogueIndex::TEXT_TEMPLATE] = _T("icon_text_search_template.png");
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
