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
#include "lazy_loader.h"

using namespace std;

static const int EVENT_LOADED_ROOT = 10000;
static const int EVENT_LOADED_MORE = 10001;

const VersionedSql* DependenciesView::GetSqlDictionary() {
  static DependenciesViewSql dict;
  return &dict;
}

static const Oid paramTypes[] = { 26 /* oid (pg_class) */, 26 /* oid */, 23 /* int4 */};

class InitialObjectInfo : public wxTreeItemData {
public:
  wxString name;
  wxString type;
};

class LoadInitialObjectWork : public DatabaseWork {
public:
  LoadInitialObjectWork(wxEvtHandler *dest, Oid regclass, Oid oid) : DatabaseWork(DependenciesView::GetSqlDictionary()), dest(dest), regclass(regclass), oid(oid) {
    wxLogDebug(_T("Work to load dependency tree root: %p"), (void*) this);
  }

protected:
  void Execute() {
    info = new InitialObjectInfo();
    info->name = _T("ROOT NAME");
    info->type = _T("ROOT TYPE");
  }
  void NotifyFinished() {
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_LOADED_ROOT);
    event.SetClientData(info);
    dest->AddPendingEvent(event);
  }

private:
  wxEvtHandler *dest;
  Oid regclass;
  Oid oid;
  InitialObjectInfo *info;
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

class DependencyResult {
public:
  DependencyResult(wxTreeItemId item, const vector<DependencyModel*> &objects) : item(item), objects(objects) {}
  wxString name;
  wxString type;
  wxTreeItemId item;
  vector<DependencyModel*> objects;
};

class LoadMoreDependenciesWork : public DatabaseWork {
public:
  LoadMoreDependenciesWork(wxEvtHandler *dest, wxTreeItemId item, bool dependenciesMode, Oid regclass, Oid oid) : DatabaseWork(DependenciesView::GetSqlDictionary()), dest(dest), item(item), dependenciesMode(dependenciesMode), regclass(regclass), oid(oid) {
    wxLogDebug(_T("Work to load dependencies: %p"), this);
  }

protected:
  void Execute() {
    QueryResults rs;
    vector<DependencyModel*> objects;
    if (DoDependenciesQuery(dependenciesMode ? _T("Dependencies") : _T("Dependents"), rs))
      for (QueryResults::iterator iter = rs.begin(); iter != rs.end(); iter++) {
	DependencyModel *dep = new DependencyModel();
	dep->deptype = ReadText(iter, 0);
	dep->regclass = ReadOid(iter, 1);
	dep->oid = ReadOid(iter, 2);
	dep->symbol = ReadText(iter, 5);
	dep->parentSubobject = ReadText(iter, 6);
	dep->childSubobject = ReadText(iter, 7);
	dep->hasMore = ReadBool(iter, 8);
	objects.push_back(dep);
      }
    result = new DependencyResult(item, objects);
  }

  bool DoDependenciesQuery(const wxString &name, QueryResults &rs) {
    wxString classValue;
    wxString objectValue;
    classValue << regclass;
    objectValue << oid;
    return DoNamedQuery(name, rs, 26 /*oid*/, 26 /*oid*/, classValue.utf8_str(), objectValue.utf8_str());
  }

  void NotifyFinished() {
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_LOADED_MORE);
    event.SetClientData(result);
    dest->AddPendingEvent(event);
  }

private:
  Oid regclass;
  Oid oid;
  int objsubid;
  wxEvtHandler *dest;
  DependencyResult *result;
  wxTreeItemId item;
  bool dependenciesMode;
};

class LoadDependenciesLazyLoader : public LazyLoader {
public:
  LoadDependenciesLazyLoader(wxEvtHandler *dest, bool dependenciesMode, DatabaseConnection *db, DependencyModel *dep) : dest(dest), dependenciesMode(dependenciesMode), db(db), dep(dep) {}
private:
  DependencyModel *dep;
  DatabaseConnection *db;
  wxEvtHandler *dest;
  bool dependenciesMode;
  void load(wxTreeItemId item) {
    db->AddWork(new LoadMoreDependenciesWork(dest, item, dependenciesMode, dep->regclass, dep->oid));
  }
};

BEGIN_EVENT_TABLE(DependenciesView, wxDialog)
  EVT_COMMAND(EVENT_LOADED_ROOT, wxEVT_COMMAND_TEXT_UPDATED, DependenciesView::OnLoadedRoot)
  EVT_COMMAND(EVENT_LOADED_MORE, wxEVT_COMMAND_TEXT_UPDATED, DependenciesView::OnLoadedDependencies)
  EVT_RADIOBOX(XRCID("DependenciesView_mode"), DependenciesView::OnChooseMode)
  EVT_TREE_ITEM_EXPANDING(wxID_ANY, DependenciesView::OnBeforeExpand)
  EVT_TREE_SEL_CHANGED(wxID_ANY, DependenciesView::OnSelectionChanged)
  EVT_BUTTON(wxID_OK, DependenciesView::OnOk)
END_EVENT_TABLE()

void DependenciesView::InitXRC(wxWindow *parent) {
  wxXmlResource::Get()->LoadDialog(this, parent, _T("DependenciesView"));
  tree = XRCCTRL(*this, "DependenciesView_dependencies", wxTreeCtrl);
  modeSelector = XRCCTRL(*this, "DependenciesView_mode", wxRadioBox);
  selectedNameCtrl = XRCCTRL(*this, "DependenciesView_selectedName", wxTextCtrl);
  selectedTypeCtrl = XRCCTRL(*this, "DependenciesView_selectedType", wxTextCtrl);
}

void DependenciesView::LoadInitialObject() {
  wxLogDebug(_T("Load initial object %lu:%lu in database %s"), (unsigned long) rootClass, (unsigned long) rootObject, db->Identification().c_str());
  db->AddWork(new LoadInitialObjectWork(this, rootClass, rootObject));
}

void DependenciesView::FillInLabels(const wxString &value) {
  SetTitle(wxString::Format(GetTitle(), value.c_str()));
  wxArrayString strings = modeSelector->GetStrings();
  for (int i = 0; i < strings.Count(); i++) {
    modeSelector->SetString(i, wxString::Format(strings[i], value.c_str()));
  }
}

void DependenciesView::OnLoadedRoot(wxCommandEvent &event) {
  wxASSERT(event.GetClientData() != NULL);
  InitialObjectInfo *rootInfo = static_cast<InitialObjectInfo*>(event.GetClientData());
  wxCHECK(rootInfo != NULL, );
  rootName = rootInfo->name;
  rootType = rootInfo->type;
  delete rootInfo;

  tree->AddRoot(rootName);
  selectedNameCtrl->SetValue(rootName);
  selectedTypeCtrl->SetValue(rootType);
  db->AddWork(new LoadMoreDependenciesWork(this, tree->GetRootItem(), mode == DEPENDENCIES, rootClass, rootObject));
}

void DependenciesView::OnLoadedDependencies(wxCommandEvent &event) {
  DependencyResult *result = static_cast< DependencyResult* >(event.GetClientData());
  wxASSERT(result);
  wxLogDebug(_T("Dependencies work finished"));
  for (vector<DependencyModel*>::iterator iter = result->objects.begin(); iter != result->objects.end(); iter++) {
    DependencyModel *dep = *iter;
    wxString itemText;
    if (!dep->parentSubobject.IsEmpty())
      itemText << wxString::Format(_("(on %s)"), dep->parentSubobject.c_str()) << _T(" ");
    itemText << dep->symbol << _T(" [deptype=") << dep->deptype << _T("]");
    if (!dep->childSubobject.IsEmpty())
      itemText << _T(" ") << wxString::Format(_("(on %s)"), dep->childSubobject.c_str());
    wxTreeItemId newItem = tree->AppendItem(result->item, itemText);
    tree->SetItemData(newItem, dep);
    if (dep->hasMore) {
      wxTreeItemId dummyItem = tree->AppendItem(newItem, _T("Loading..."));
      LazyLoader *loader = new LoadDependenciesLazyLoader(this, mode == DEPENDENCIES, db, dep);
      tree->SetItemData(dummyItem, loader);
    }
  }
  wxString label = tree->GetItemText(result->item);
  label = label.Left(label.length() - wxString(_(" (loading...)")).length());
  tree->SetItemText(result->item, label);
  tree->Expand(result->item);
  delete result;
}

void DependenciesView::OnChooseMode(wxCommandEvent &event) {
  if (event.GetSelection() == 0 && mode == DEPENDENCIES) {
    wxLogDebug(_T("Change to dependents mode"));
    mode = DEPENDENTS;
    tree->DeleteAllItems();
    LoadInitialObject();
  }
  else if (event.GetSelection() == 1 && mode == DEPENDENTS) {
    wxLogDebug(_T("Change to dependencies mode"));
    mode = DEPENDENCIES;
    tree->DeleteAllItems();
    LoadInitialObject();
  }
}

void DependenciesView::OnBeforeExpand(wxTreeEvent &event) {
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

void DependenciesView::OnSelectionChanged(wxTreeEvent &event) {
  if (event.GetItem() == tree->GetRootItem()) {
    selectedNameCtrl->SetValue(rootName);
    selectedTypeCtrl->SetValue(rootType);
  }
  else {
    wxTreeItemData *itemData = tree->GetItemData(event.GetItem());
    wxASSERT(itemData != NULL);
    DependencyModel *dep = dynamic_cast<DependencyModel*>(itemData);
    wxCHECK(dep != NULL, );
    selectedNameCtrl->SetValue(dep->symbol);
    selectedTypeCtrl->SetValue(dep->type);
  }
}
