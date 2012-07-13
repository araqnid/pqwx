/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __dependencies_view_h
#define __dependencies_view_h

#include "libpq-fe.h"
#include "database_connection.h"
#include "sql_dictionary.h"
#include "work_launcher.h"
#include "object_browser_managed_work.h"
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
  DependenciesView(wxWindow *parent, WorkLauncher *launcher, const wxString &label, const ObjectModelReference& ref) : wxDialog(), launcher(launcher), rootRef(ref) {
    InitXRC(parent);
    mode = DEPENDENTS;
    FillInLabels(label);
    LoadInitialObject();
  }

private:
  class Work;

  typedef void (DependenciesView::*WorkCompleted)(Work*);

  class Work : public ObjectBrowserManagedWork {
  public:
    Work(const ObjectModelReference& databaseRef, DependenciesView* dest, WorkCompleted completionHandler, WorkCompleted crashHandler = NULL) : ObjectBrowserManagedWork(READ_ONLY, databaseRef, GetSqlDictionary(), dest), completionHandler(completionHandler), crashHandler(crashHandler) {}
    const WorkCompleted completionHandler;
    const WorkCompleted crashHandler;
  };

  class LoadInitialObjectWork : public Work {
  public:
    LoadInitialObjectWork(const ObjectModelReference& databaseRef, Oid regclass, Oid oid, DependenciesView *dest) : Work(databaseRef, dest, &DependenciesView::OnLoadedRoot)
    {
      wxLogDebug(_T("Work to load dependency tree root: %p"), (void*) this);
    }

    wxString name, type;
  protected:
    void operator()();

  private:
    wxEvtHandler *dest;
    Oid regclass;
    Oid oid;
  };

  class DependencyModel : public wxTreeItemData {
  public:
    wxString deptype;
    wxString type;
    Oid regclass;
    Oid oid;
    wxString parentSubobject;
    wxString childSubobject;
    wxString symbol;
    bool hasMore;
  };

  class LoadMoreDependenciesWork : public Work {
  public:
    LoadMoreDependenciesWork(const ObjectModelReference& databaseRef, wxTreeItemId item, bool dependenciesMode, Oid regclass, Oid oid, Oid database, DependenciesView *dest) : Work(databaseRef, dest, &DependenciesView::OnLoadedDependencies), item(item), dependenciesMode(dependenciesMode), regclass(regclass), oid(oid), database(database)
    {
      wxLogDebug(_T("Work to load dependencies: %p"), this);
    }

    wxString name;
    wxString type;
    std::vector<DependencyModel> objects;

  protected:
    void operator()();

  private:
    wxEvtHandler *dest;
    wxTreeItemId item;
    bool dependenciesMode;
    Oid regclass;
    Oid oid;
    Oid database;
    int objsubid;

    friend class DependenciesView;
  };

  DECLARE_EVENT_TABLE();

  WorkLauncher* const launcher;
  const ObjectModelReference rootRef;

  void OnLoadedRoot(Work*);
  void OnLoadedDependencies(Work*);
  void OnWorkFinished(wxCommandEvent &event);
  void OnWorkCrashed(wxCommandEvent &event);
  void OnChooseMode(wxCommandEvent &event);
  void OnBeforeExpand(wxTreeEvent &event);
  void OnSelectionChanged(wxTreeEvent &event);
  void OnOk(wxCommandEvent &event) { Destroy(); }

  static const SqlDictionary& GetSqlDictionary();

  wxTreeCtrl *tree;
  wxRadioBox *modeSelector;
  wxTextCtrl *selectedNameCtrl, *selectedTypeCtrl;
  void InitXRC(wxWindow *parent);
  void LoadInitialObject();
  void FillInLabels(const wxString &label);
  enum { DEPENDENTS, DEPENDENCIES } mode;

  wxString rootName;
  wxString rootType;

  friend class LoadDependenciesLazyLoader;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
