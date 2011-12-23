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
    Init(parent);

    filter &= catalogue->CreateTypeFilter(CatalogueIndex::TABLE)
	       | catalogue->CreateTypeFilter(CatalogueIndex::VIEW)
	       | catalogue->CreateTypeFilter(CatalogueIndex::SEQUENCE)
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
  void Init(wxWindow *parent);
  std::map<CatalogueIndex::Type, wxString> iconMap;

  wxString findIcon(CatalogueIndex::Type documentType) {
    std::map<CatalogueIndex::Type, wxString>::const_iterator ptr = iconMap.find(documentType);
    if (ptr == iconMap.end()) return wxEmptyString;
    return ptr->second;
  }

  DECLARE_EVENT_TABLE();
};

#endif
