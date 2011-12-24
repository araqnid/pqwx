// -*- mode: c++ -*-

#ifndef __dependencies_view_h
#define __dependencies_view_h

#include "libpq-fe.h"
#include "database_connection.h"
#include "versioned_sql.h"

class wxTreeCtrl;

class DependenciesView : public wxDialog {
public:
  DependenciesView(wxWindow *parent, DatabaseConnection *db, Oid regclass, Oid oid, int objsubid = 0) : wxDialog(), db(db) {
    InitXRC(parent);
    LoadInitialObject(regclass, oid, objsubid);
  }
  virtual ~DependenciesView() {
  }

  void OnOk(wxCommandEvent &event) { Destroy(); }
  void OnWorkFinished(wxCommandEvent &event);
  void BeforeExpand(wxTreeEvent &event);
  static const VersionedSql& GetSqlDictionary();

private:
  DECLARE_EVENT_TABLE();
  DatabaseConnection *db;
  wxTreeCtrl *tree;
  void InitXRC(wxWindow *parent);
  void LoadInitialObject(Oid regclass, Oid oid, int subid);
};

#endif
