/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_finder_h
#define __object_finder_h

#include <vector>
#include "wx/dialog.h"
#include "wx/xrc/xmlres.h"
#include "wx/htmllbox.h"
#include "catalogue_index.h"
#include "pqwx_frame.h"

/**
 * Dialogue box to find a database object from an abbreviated name.
 *
 * The user can enter something like "MyTab" in the query field to match "public.my_table", for example.
 */
class ObjectFinder : public wxDialog {
public:
  /**
   * Callback interface for non-modal use of object finder dialogue.
   *
   * Implementations <b>must</b> be allocated on the heap as they will
   * be deleted just after one of the callback methods is called.
   */
  class Completion {
  public:
    /**
     * Object chosen in dialogue box.
     */
    virtual void OnObjectChosen(const CatalogueIndex::Document*) = 0;
    /**
     * Object find cancelled.
     */
    virtual void OnCancelled() = 0;
  };

  /**
   * Create filter for object finder results.
   *
   * This filter removes system objects and trigger functions.
   */
  static CatalogueIndex::Filter CreateFilter(const CatalogueIndex *catalogue) {
    return catalogue->CreateNonSystemFilter()
      & (catalogue->CreateTypeFilter(CatalogueIndex::TABLE)
	 | catalogue->CreateTypeFilter(CatalogueIndex::VIEW)
	 | catalogue->CreateTypeFilter(CatalogueIndex::SEQUENCE)
	 | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_SCALAR)
	 | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_ROWSET)
	 | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_AGGREGATE)
	 | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_WINDOW)
	 );
  }

  /**
   * Create object finder, specifying catalogue index.
   *
   * This constructor is for a modal dialogue.
   */
  ObjectFinder(wxWindow *parent, const CatalogueIndex *catalogue)
    : wxDialog(), catalogue(catalogue), completion(NULL), filter(CreateFilter(catalogue)) {
    Init(parent);
  }

  /**
   * Create object finder, specifying catalogue index.
   *
   * This constructor is for a non-modal dialogue.
   */
  ObjectFinder(wxWindow *parent, const CatalogueIndex *catalogue, Completion *callback)
    : wxDialog(), catalogue(catalogue), completion(callback), filter(CreateFilter(catalogue)) {
    Init(parent);
  }

  ~ObjectFinder() {
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

// Local Variables:
// mode: c++
// End:
