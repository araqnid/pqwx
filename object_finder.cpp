#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <vector>
#include "wx/xrc/xmlres.h"
#include "wx/config.h"
#include "object_finder.h"
#include "object_browser.h"

using namespace std;

BEGIN_EVENT_TABLE(ObjectFinder, wxDialog)
  EVT_TEXT(XRCID("query"), ObjectFinder::OnQueryChanged)
  EVT_BUTTON(wxID_OK, ObjectFinder::OnOk)
  EVT_BUTTON(wxID_CANCEL, ObjectFinder::OnCancel)
  EVT_CLOSE(ObjectFinder::OnClose)
  EVT_LISTBOX_DCLICK(XRCID("results"), ObjectFinder::OnDoubleClickResult)
END_EVENT_TABLE()

void ObjectFinder::OnQueryChanged(wxCommandEvent &event) {
  wxString query = queryInput->GetValue();

  resultsCtrl->Clear();

  if (!query.IsEmpty()) {
    results = catalogue->Search(query, filter);
  }
  else {
    results.clear();
  }

  for (vector<CatalogueIndex::Result>::iterator iter = results.begin(); iter != results.end(); iter++) {
    unsigned n = resultsCtrl->Append(iter->document->symbol);
    resultsCtrl->SetClientData(n, &(*iter));
  }
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
  void *clientData = resultsCtrl->GetClientData(n);
  wxASSERT(clientData != NULL);
  CatalogueIndex::Result *result = static_cast<CatalogueIndex::Result*>(clientData);
  wxLogDebug(_T("Open object: %s"), result->document->symbol.c_str());
  completion->OnObjectChosen(result->document);

  Destroy();
}

void ObjectFinder::OnCancel(wxCommandEvent &event) {
  Destroy();
}

void ObjectFinder::OnClose(wxCloseEvent &event) {
  Destroy();
}
