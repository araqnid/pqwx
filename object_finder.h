// -*- mode: c++ -*-

#ifndef __object_finder_h
#define __object_finder_h

#include <vector>
#include "wx/dialog.h"
#include "wx/xrc/xmlres.h"
#include "wx/htmllbox.h"
#include "catalogue_index.h"
#include "pqwx_frame.h"

class ObjectFinder : public wxDialog {
public:
  class Completion {
  public:
    virtual void OnObjectChosen(const CatalogueIndex::Document*) = 0;
    virtual void OnCancelled() = 0;
  };

  ObjectFinder(wxWindow *parent, Completion *callback, const CatalogueIndex *catalogue)
    : wxDialog(), catalogue(catalogue), completion(callback), filter(catalogue->CreateNonSystemFilter()) {
    InitXRC(parent);

    filter &= catalogue->CreateTypeFilter(CatalogueIndex::TABLE)
	       | catalogue->CreateTypeFilter(CatalogueIndex::VIEW) 
	       | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_SCALAR)
	       | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_AGGREGATE)
	       | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_WINDOW)
	       ;
  }
  virtual ~ObjectFinder() {
    delete completion;
  }

  void OnQueryChanged(wxCommandEvent&);
  void OnOk(wxCommandEvent&);
  void OnDoubleClickResult(wxCommandEvent&);
  void OnCancel(wxCommandEvent&);
  void OnClose(wxCloseEvent&);

protected:
  wxTextCtrl *queryInput;
  wxSimpleHtmlListBox *resultsCtrl;

private:
  const CatalogueIndex *catalogue;
  Completion *completion;
  CatalogueIndex::Filter filter;
  std::vector<CatalogueIndex::Result> results;
  void InitXRC(wxWindow *parent) {
    wxXmlResource::Get()->LoadDialog(this, parent, _T("ObjectFinder"));
    queryInput = XRCCTRL(*this, "query", wxTextCtrl);
    // bodge-tastic... xrced doesn't support wxSimpleHtmlListBox
    wxListBox *dummyResultsCtrl = XRCCTRL(*this, "results", wxListBox);
    resultsCtrl = new wxSimpleHtmlListBox(this, Pqwx_ObjectFinderResults);
    GetSizer()->Replace(dummyResultsCtrl, resultsCtrl);
    dummyResultsCtrl->Destroy();
  }

  DECLARE_EVENT_TABLE();
};

#endif
