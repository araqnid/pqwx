#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <sys/time.h>
#include <vector>
#include "wx/treectrl.h"
#include "wx/xrc/xmlres.h"
#include "dependencies_view.h"
#include "database_work.h"
#include "dependencies_view_sql.h"
#include "query_results.h"
#include "lazy_loader.h"

using namespace std;

static const int EVENT_WORK_FINISHED = 10000;

const VersionedSql& DependenciesView::GetSqlDictionary() {
  static DependenciesViewSql dict;
  return dict;
}

static const Oid paramTypes[] = { 26 /* oid (pg_class) */, 26 /* oid */, 23 /* int4 */};

struct DependencyModel {
  wxString deptype;
  wxString type;
  Oid regclass;
  Oid oid;
  int objsubid;
  int refobjsubid;
  wxString symbol;
  bool hasMore;
};

class DependencyResult {
public:
  DependencyResult(wxTreeItemId item, const vector<DependencyModel> &objects) : item(item), objects(objects) {}
  wxTreeItemId item;
  vector<DependencyModel> objects;
};

class DependenciesViewWork : public DatabaseWork {
protected:
  DependenciesViewWork(wxEvtHandler *dest, wxTreeItemId item, Oid regclass, Oid oid, int objsubid) : dest(dest), item(item), regclass(regclass), oid(oid), objsubid(objsubid) {}
  DependencyResult *result;
  bool DoQuery(PGconn *conn, const wxString name, QueryResults &results) {
    const char *sql = GetSql(conn, name);

    logger->LogSql(sql);

#ifdef PQWX_DEBUG
    struct timeval start;
    gettimeofday(&start, NULL);
#endif

    const wxCharBuffer value0 = wxString::Format(_T("%u"), regclass).utf8_str();
    const wxCharBuffer value1 = wxString::Format(_T("%u"), oid).utf8_str();
    const wxCharBuffer value2 = wxString::Format(_T("%u"), objsubid).utf8_str();
    const char * paramValues[3];
    paramValues[0] = value0.data();
    paramValues[1] = value1.data();
    paramValues[2] = value2.data();

    PGresult *rs = PQexecParams(conn, sql, 3, (const Oid*) &paramTypes, (const char * const * ) &paramValues, NULL, NULL, 0);
    if (!rs)
      return false;

#ifdef PQWX_DEBUG
    struct timeval finish;
    gettimeofday(&finish, NULL);
    struct timeval elapsed;
    timersub(&finish, &start, &elapsed);
    double elapsedFP = (double) elapsed.tv_sec + ((double) elapsed.tv_usec / 1000000.0);
    wxLogDebug(_T("(%.4lf seconds)"), elapsedFP);
#endif

    ExecStatusType status = PQresultStatus(rs);
    if (status != PGRES_TUPLES_OK) {
#ifdef PQWX_DEBUG
      logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
      return false; // expected data back
    }

    ReadResultSet(rs, results);

    PQclear(rs);

    return true;
  }
  wxTreeItemId item;
  virtual DependencyResult *FindDependencies(PGconn *conn) = 0;
private:
  void Execute(PGconn *conn) {
    result = FindDependencies(conn);
  }
  Oid regclass;
  Oid oid;
  int objsubid;
  wxEvtHandler *dest;
  void ReadResultSet(PGresult *rs, QueryResults &results) {
    int rowCount = PQntuples(rs);
    int colCount = PQnfields(rs);
    results.reserve(rowCount);
    for (int rowNum = 0; rowNum < rowCount; rowNum++) {
      vector<wxString> row;
      row.reserve(colCount);
      for (int colNum = 0; colNum < colCount; colNum++) {
	const char *value = PQgetvalue(rs, rowNum, colNum);
	row.push_back(wxString(value, wxConvUTF8));
      }
      results.push_back(row);
    }
  }
  void NotifyFinished() {
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_WORK_FINISHED);
    event.SetClientData(result);
    dest->AddPendingEvent(event);
  }
  const char *GetSql(PGconn *conn, const wxString &name) const {
    return DependenciesView::GetSqlDictionary().GetSql(name, PQserverVersion(conn));
  }
};

class LoadDependentsWork : public DependenciesViewWork {
public:
  LoadDependentsWork(wxEvtHandler *dest, wxTreeItemId item, Oid regclass, Oid oid, int objsubid) : DependenciesViewWork(dest, item, regclass, oid, objsubid) {
    wxLogDebug(_T("Work to load dependents: %p"), this);
  }
private:
  DependencyResult *FindDependencies(PGconn *conn) {
    QueryResults rs;
    vector<DependencyModel> objects;
    if (DoQuery(conn, _T("Dependents"), rs))
      for (QueryResults::iterator iter = rs.begin(); iter != rs.end(); iter++) {
	DependencyModel dep;
	dep.deptype = ReadText(iter, 0);
	dep.regclass = ReadOid(iter, 1);
	dep.oid = ReadOid(iter, 2);
	dep.objsubid = ReadOid(iter, 3);
	dep.refobjsubid = ReadOid(iter, 4);
	dep.symbol = ReadText(iter, 5);
	objects.push_back(dep);
      }
    return new DependencyResult(item, objects);
  }
};

class LoadDependentsLazyLoader : public LazyLoader {
public:
  LoadDependentsLazyLoader(wxEvtHandler *dest, DatabaseConnection *db, DependencyModel dep) : dest(dest), db(db), dep(dep) {}
private:
  DependencyModel dep;
  DatabaseConnection *db;
  wxEvtHandler *dest;
  void load(wxTreeItemId item) {
    db->AddWork(new LoadDependentsWork(dest, item, dep.regclass, dep.oid, dep.objsubid));
  }
};

class LoadDependenciesWork : public DependenciesViewWork {
public:
  LoadDependenciesWork(wxEvtHandler *dest, wxTreeItemId item, Oid regclass, Oid oid, int objsubid) : DependenciesViewWork(dest, item, regclass, oid, objsubid) {
    wxLogDebug(_T("Work to load dependencies: %p"), this);
  }
private:
  DependencyResult *FindDependencies(PGconn *conn) {
    QueryResults rs;
    vector<DependencyModel> objects;
    if (DoQuery(conn, _T("Dependents"), rs))
      for (QueryResults::iterator iter = rs.begin(); iter != rs.end(); iter++) {
	DependencyModel dep;
	dep.deptype = ReadText(iter, 0);
	dep.regclass = ReadOid(iter, 1);
	dep.oid = ReadOid(iter, 2);
	dep.objsubid = ReadOid(iter, 3);
	dep.refobjsubid = ReadOid(iter, 4);
	dep.symbol = ReadText(iter, 5);
	objects.push_back(dep);
      }
    return new DependencyResult(item, objects);
  }
};

BEGIN_EVENT_TABLE(DependenciesView, wxDialog)
  EVT_BUTTON(wxID_OK, DependenciesView::OnOk)
  EVT_COMMAND(EVENT_WORK_FINISHED, wxEVT_COMMAND_TEXT_UPDATED, DependenciesView::OnWorkFinished)
  EVT_TREE_ITEM_EXPANDING(wxID_ANY, DependenciesView::BeforeExpand)
END_EVENT_TABLE()

void DependenciesView::InitXRC(wxWindow *parent) {
  wxXmlResource::Get()->LoadDialog(this, parent, _T("DependenciesView"));
  tree = XRCCTRL(*this, "DependenciesView_dependencies", wxTreeCtrl);
}

void DependenciesView::LoadInitialObject(Oid regclass, Oid oid, int subid) {
  wxLogDebug(_T("Load initial object %lu in database %s"), (unsigned long) oid, db->Identification().c_str());
  tree->AddRoot(_T("root"));
  db->AddWork(new LoadDependentsWork(this, tree->GetRootItem(), regclass, oid, subid));
}

void DependenciesView::OnWorkFinished(wxCommandEvent &event) {
  DependencyResult *result = static_cast< DependencyResult* >(event.GetClientData());
  wxASSERT(result);
  wxLogDebug(_T("Dependencies work finished"));
  for (vector<DependencyModel>::iterator iter = result->objects.begin(); iter != result->objects.end(); iter++) {
    wxLogDebug(_T("Found %s"), (*iter).symbol.c_str());
    wxTreeItemId newItem = tree->AppendItem(result->item, (*iter).symbol + _T(" [deptype=") + (*iter).deptype + _T("]"));
    if ((*iter).hasMore) {
      wxTreeItemId dummyItem = tree->AppendItem(newItem, _T("Loading..."));
      LazyLoader *loader = new LoadDependentsLazyLoader(this, db, (*iter));
      tree->SetItemData(dummyItem, loader);
    }
  }
  tree->Expand(result->item);
  delete result;
}

void DependenciesView::BeforeExpand(wxTreeEvent &event) {
  wxTreeItemIdValue cookie;
  wxTreeItemId expandingItem = event.GetItem();
  wxTreeItemId firstChildItem = tree->GetFirstChild(expandingItem, cookie);
  wxTreeItemData *itemData = tree->GetItemData(firstChildItem);
  if (itemData == NULL) return;
  LazyLoader *lazyLoader = dynamic_cast<LazyLoader*>(itemData);
  if (lazyLoader != NULL) {
    lazyLoader->load(expandingItem);
    wxString expandingItemText = tree->GetItemText(expandingItem);
    tree->SetItemText(expandingItem, expandingItemText + _(" (loading...)"));
    tree->Delete(firstChildItem);
    // veto expand event, for lazy loader to complete
    event.Veto();
  }
}
