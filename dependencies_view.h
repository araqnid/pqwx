// -*- mode: c++ -*-

#ifndef __dependencies_view_h
#define __dependencies_view_h

#include "libpq-fe.h"
#include "database_connection.h"
#include "versioned_sql.h"

class wxTreeCtrl;

class DependenciesView : public wxDialog {
public:
  DependenciesView(wxWindow *parent, DatabaseConnection *db, const wxString &label, Oid regclass, Oid oid) : wxDialog(), db(db), rootClass(regclass), rootObject(oid) {
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

  static const VersionedSql* GetSqlDictionary();

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
  wxString rootName;
  wxString rootType;
};

#endif
