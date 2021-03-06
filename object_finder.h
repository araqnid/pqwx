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
    virtual ~Completion() { }
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
   * This filter removes trigger functions.
   */
  static CatalogueIndex::Filter CreateTypesFilter(const CatalogueIndex *catalogue) {
    return catalogue->CreateTypeFilter(CatalogueIndex::TABLE)
      | catalogue->CreateTypeFilter(CatalogueIndex::TABLE_UNLOGGED)
      | catalogue->CreateTypeFilter(CatalogueIndex::VIEW)
      | catalogue->CreateTypeFilter(CatalogueIndex::SEQUENCE)
      | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_SCALAR)
      | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_ROWSET)
      | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_AGGREGATE)
      | catalogue->CreateTypeFilter(CatalogueIndex::FUNCTION_WINDOW)
      | catalogue->CreateTypeFilter(CatalogueIndex::TEXT_CONFIGURATION)
      | catalogue->CreateTypeFilter(CatalogueIndex::TEXT_DICTIONARY)
      | catalogue->CreateTypeFilter(CatalogueIndex::TEXT_PARSER)
      | catalogue->CreateTypeFilter(CatalogueIndex::TEXT_TEMPLATE)
      ;
  }

  /**
   * Create object finder, specifying catalogue index.
   */
  ObjectFinder(wxWindow *parent, const CatalogueIndex *catalogue, Completion *callback = NULL)
    : wxDialog(), catalogue(catalogue), completion(callback),
      nonSystemFilter(catalogue->CreateNonSystemFilter()),
      nonExtensionFilter(catalogue->CreateNonExtensionFilter()),
      typesFilter(CreateTypesFilter(catalogue))
  {
    Init(parent);
  }

  virtual ~ObjectFinder()
  {
    if (completion != NULL) delete completion;
  }

  void OnQueryChanged(wxCommandEvent& e) { SearchCatalogue(); }
  void OnOk(wxCommandEvent&);
  void OnDoubleClickResult(wxCommandEvent&);
  void OnCancel(wxCommandEvent&);
  void OnClose(wxCloseEvent&);
  void OnIncludeExtensions(wxCommandEvent& e) { SearchCatalogue(); }
  void OnIncludeSystem(wxCommandEvent& e) { SearchCatalogue(); }

  void MoveUp() { resultsCtrl->MoveUp(); }
  void MoveDown() { resultsCtrl->MoveDown(); }

  void SearchCatalogue();

private:
  class TextQueryControl : public wxTextCtrl {
  public:
    TextQueryControl(ObjectFinder *owner, wxWindowID id) : wxTextCtrl(owner,id), owner(owner) { }
    void OnCharacterInput(wxKeyEvent& event);
  private:
    ObjectFinder * const owner;
    DECLARE_EVENT_TABLE();
  };

  class ResultsControl : public wxSimpleHtmlListBox {
  public:
    ResultsControl(ObjectFinder *owner, wxWindowID id) : wxSimpleHtmlListBox(owner,id) { }
    void MoveUp();
    void MoveDown();
  };

protected:
  TextQueryControl *queryInput;
  ResultsControl *resultsCtrl;
  wxCheckBox *includeSystemInput;
  wxCheckBox *includeExtensionsInput;

private:
  const CatalogueIndex *catalogue;
  Completion *completion;
  const CatalogueIndex::Filter nonSystemFilter;
  const CatalogueIndex::Filter nonExtensionFilter;
  const CatalogueIndex::Filter typesFilter;
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
// indent-tabs-mode: nil
// End:
