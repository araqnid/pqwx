/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __dependencies_view_h
#define __dependencies_view_h

#include "libpq-fe.h"
#include "database_connection.h"
#include "versioned_sql.h"
#include "wx/dialog.h"
#include "wx/treectrl.h"
#include "wx/radiobox.h"

/**
 * Dialogue box to show dependencies/dependents of database objects.
 */
class DependenciesView : public wxDialog {
public:
  /**
   * Create dependencies dialogue, starting with some specified object.
   */
  DependenciesView(wxWindow *parent, DatabaseConnection *db, const wxString &label, Oid regclass, Oid oid, Oid database) : wxDialog(), db(db), rootClass(regclass), rootObject(oid), database(database) {
    InitXRC(parent);
    mode = DEPENDENTS;
    FillInLabels(label);
    LoadInitialObject();
  }
  virtual ~DependenciesView() {
  }

  void OnLoadedRoot(wxCommandEvent &event);
  void OnLoadedDependencies(wxCommandEvent &event);
  void OnChooseMode(wxCommandEvent &event);
  void OnBeforeExpand(wxTreeEvent &event);
  void OnSelectionChanged(wxTreeEvent &event);
  void OnOk(wxCommandEvent &event) { Destroy(); }

  static const VersionedSql& GetSqlDictionary();

private:
  DECLARE_EVENT_TABLE();
  DatabaseConnection *db;
  wxTreeCtrl *tree;
  wxRadioBox *modeSelector;
  wxTextCtrl *selectedNameCtrl, *selectedTypeCtrl;
  void InitXRC(wxWindow *parent);
  void LoadInitialObject();
  void FillInLabels(const wxString &label);
  enum { DEPENDENTS, DEPENDENCIES } mode;
  Oid rootClass;
  Oid rootObject;
  Oid database;
  wxString rootName;
  wxString rootType;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
