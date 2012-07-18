#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <vector>
#include "wx/treectrl.h"
#include "wx/xrc/xmlres.h"
#include "dependencies_view.h"
#include "database_work.h"
#include "lazy_loader.h"

static const Oid paramTypes[] = { 26 /* oid (pg_class) */, 26 /* oid */, 23 /* int4 */};

class LoadDependenciesLazyLoader : public LazyLoader {
public:
  LoadDependenciesLazyLoader(DependenciesView *dest, bool dependenciesMode, DependenciesView::DependencyModel *dep) : dest(dest), dependenciesMode(dependenciesMode), dep(dep) {}
private:
  DependenciesView * const dest;
  const bool dependenciesMode;
  DependenciesView::DependencyModel * const dep;
  bool load(wxTreeItemId item)
  {
    dest->launcher->DoWork(new DependenciesView::LoadMoreDependenciesWork(item, dependenciesMode, ObjectModelReference(dest->rootRef.DatabaseRef(), dep->regclass, dep->oid), dest));
    return true;
  }
};

BEGIN_EVENT_TABLE(DependenciesView, wxDialog)
  PQWX_OBJECT_BROWSER_WORK_FINISHED(wxID_ANY, DependenciesView::OnWorkFinished)
  PQWX_OBJECT_BROWSER_WORK_CRASHED(wxID_ANY, DependenciesView::OnWorkCrashed)
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

#define CALL_WORK_COMPLETION(dbox, handler) ((dbox).*(handler))

void DependenciesView::OnWorkFinished(wxCommandEvent& event)
{
  Work *work = static_cast<Work*>(event.GetClientData());
  wxASSERT(work != NULL);
  wxLogDebug(_T("%p: work finished (on dependencies view)"), work);
  CALL_WORK_COMPLETION(*this, work->completionHandler)(work);
  delete work;
}

void DependenciesView::OnWorkCrashed(wxCommandEvent& event)
{
  Work *work = static_cast<Work*>(event.GetClientData());
  wxLogDebug(_T("%p: work crashed (on dependencies view)"), work);

  if (!work->GetCrashMessage().empty()) {
    wxLogError(_T("%s\n%s"), _("An unexpected error occurred interacting with the database. Failure will ensue."), work->GetCrashMessage().c_str());
  }
  else {
    wxLogError(_T("%s"), _("An unexpected and unidentified error occurred interacting with the database. Failure will ensue."));
  }

  if (work->crashHandler != NULL)
    CALL_WORK_COMPLETION(*this, work->crashHandler)(work);

  delete work;
}

void DependenciesView::LoadInitialObject() {
  wxLogDebug(_T("Load initial object %s"), rootRef.Identify().c_str());
  launcher->DoWork(new LoadInitialObjectWork(rootRef, this));
}

void DependenciesView::FillInLabels(const wxString &value) {
  SetTitle(wxString::Format(GetTitle(), value.c_str()));
  wxArrayString strings = modeSelector->GetStrings();
  for (unsigned i = 0; i < strings.Count(); i++) {
    modeSelector->SetString(i, wxString::Format(strings[i], value.c_str()));
  }
}

void DependenciesView::LoadInitialObjectWork::DoManagedWork()
{
  name = _T("ROOT NAME");
  type = _T("ROOT TYPE");
}

void DependenciesView::OnLoadedRoot(Work* work) {
  LoadInitialObjectWork* loadWork = static_cast<LoadInitialObjectWork*>(work);
  rootName = loadWork->name;
  rootType = loadWork->type;

  tree->AddRoot(rootName);
  selectedNameCtrl->SetValue(rootName);
  selectedTypeCtrl->SetValue(rootType);

  launcher->DoWork(new LoadMoreDependenciesWork(tree->GetRootItem(), mode == DEPENDENCIES, rootRef, this));
}

void DependenciesView::LoadMoreDependenciesWork::DoManagedWork()
{
  wxString queryName = dependenciesMode ? _T("Dependencies") : _T("Dependents");
  QueryResults rs = Query(queryName).OidParam(ref.GetObjectClass()).OidParam(ref.GetOid()).OidParam(ref.GetDatabase()).List();
  for (QueryResults::rows_iterator iter = rs.Rows().begin(); iter != rs.Rows().end(); iter++) {
    DependencyModel dep;
    dep.deptype = (*iter).ReadText(0);
    dep.regclass = (*iter).ReadOid(1);
    dep.oid = (*iter).ReadOid(2);
    dep.symbol = (*iter).ReadText(5);
    dep.parentSubobject = (*iter).ReadText(6);
    dep.childSubobject = (*iter).ReadText(7);
    dep.hasMore = (*iter).ReadBool(8);
    objects.push_back(dep);
  }
}

void DependenciesView::OnLoadedDependencies(Work* work) {
  LoadMoreDependenciesWork* loadWork = static_cast<LoadMoreDependenciesWork*>(work);
  wxLogDebug(_T("Dependencies work finished"));
  for (std::vector<DependencyModel>::iterator iter = loadWork->objects.begin(); iter != loadWork->objects.end(); iter++) {
    DependencyModel *dep = new DependencyModel(*iter);
    wxString itemText;
    if (!dep->parentSubobject.IsEmpty())
      itemText << wxString::Format(_("(on %s)"), dep->parentSubobject.c_str()) << _T(" ");
    itemText << dep->symbol << _T(" [deptype=") << dep->deptype << _T("]");
    if (!dep->childSubobject.IsEmpty())
      itemText << _T(" ") << wxString::Format(_("(on %s)"), dep->childSubobject.c_str());
    wxTreeItemId newItem = tree->AppendItem(loadWork->item, itemText);
    tree->SetItemData(newItem, dep);
    if (dep->hasMore) {
      wxTreeItemId dummyItem = tree->AppendItem(newItem, _T("Loading..."));
      LazyLoader *loader = new LoadDependenciesLazyLoader(this, mode == DEPENDENCIES, dep);
      tree->SetItemData(dummyItem, loader);
    }
  }
  wxString label = tree->GetItemText(loadWork->item);
  label = label.Left(label.length() - wxString(_(" (loading...)")).length());
  tree->SetItemText(loadWork->item, label);
  tree->Expand(loadWork->item);
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

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
