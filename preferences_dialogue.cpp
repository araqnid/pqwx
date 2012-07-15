#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/xrc/xmlres.h"
#include "wx/config.h"

#include "pqwx.h"
#include "preferences_dialogue.h"
#include "pg_tools_registry.h"

BEGIN_EVENT_TABLE(PreferencesDialogue, wxDialog)
  EVT_LIST_ITEM_SELECTED(XRCID("InstallationsList"), PreferencesDialogue::OnInstallationSelected)
  EVT_LIST_ITEM_DESELECTED(XRCID("InstallationsList"), PreferencesDialogue::OnInstallationDeselected)
  EVT_BUTTON(XRCID("RemoveInstallation"), PreferencesDialogue::OnRemoveInstallation)
  EVT_BUTTON(XRCID("AddInstallation"), PreferencesDialogue::OnAddInstallation)
  EVT_TOOLS_REGISTRY(XRCID("InstallationsList"), PQWX_ToolsRegistryUpdated, PreferencesDialogue::OnToolsRegistryUpdated)

  EVT_BUTTON(wxID_OK, PreferencesDialogue::OnFinishPreferences)
  EVT_BUTTON(wxID_CANCEL, PreferencesDialogue::OnCancelPreferences)
END_EVENT_TABLE()

PreferencesDialogue::PreferencesDialogue(wxWindow* parent) : registry(::wxGetApp().GetToolsRegistry())
{
  InitXRC(parent);
  LoadInstallations();
}

void PreferencesDialogue::InitXRC(wxWindow* parent)
{
  wxXmlResource::Get()->LoadDialog(this, parent, _T("preferences"));
  installationsList = XRCCTRL(*this, "InstallationsList", wxListCtrl);
  addInstallationButton = XRCCTRL(*this, "AddInstallation", wxButton);
  removeInstallationButton = XRCCTRL(*this, "RemoveInstallation", wxButton);
}

void PreferencesDialogue::OnFinishPreferences(wxCommandEvent& event)
{
  SaveInstallations();
  event.Skip();
}

void PreferencesDialogue::OnCancelPreferences(wxCommandEvent& event)
{
  CancelInstallations();
  event.Skip();
}

void PreferencesDialogue::LoadInstallations()
{
  installationsList->ClearAll();

  wxListItem column;
  column.SetText(_("Version"));
  column.SetAlign(wxLIST_FORMAT_LEFT);
  installationsList->InsertColumn(0, column);
  column.SetText(_("Type"));
  installationsList->InsertColumn(1, column);
  column.SetText(_("Path"));
  installationsList->InsertColumn(2, column);

  int index = 0;
  for (std::list<PgToolsRegistry::Installation>::const_iterator iter = registry.begin(); iter != registry.end(); iter++, index++) {
    long rowNumber = installationsList->InsertItem(index, (*iter).Version());
    installationsList->SetItemData(rowNumber, index);
    switch ((*iter).GetType()) {
    case PgToolsRegistry::SYSTEM:
      installationsList->SetItem(rowNumber, 1, _T("Automatic"));
      break;
    case PgToolsRegistry::USER:
      installationsList->SetItem(rowNumber, 1, _T("User-specified"));
      break;
    }
    installationsList->SetItem(rowNumber, 2, (*iter).Path());
  }

  installationsList->SortItems(CollateInstallations, 0);
  installationsList->SetColumnWidth(0, wxLIST_AUTOSIZE);
  installationsList->SetColumnWidth(1, wxLIST_AUTOSIZE);
  installationsList->SetColumnWidth(2, wxLIST_AUTOSIZE);
}

int wxCALLBACK PreferencesDialogue::CollateInstallations(long itemData1, long itemData2, long sortData)
{
  PgToolsRegistry& registry = ::wxGetApp().GetToolsRegistry();
  int index = 0;
  const PgToolsRegistry::Installation *inst1 = NULL;
  const PgToolsRegistry::Installation *inst2 = NULL;
  for (PgToolsRegistry::const_iterator iter = registry.begin(); iter != registry.end(); iter++, index++) {
    if (index == itemData1) inst1 = &(*iter);
    if (index == itemData2) inst2 = &(*iter);
  }
  wxASSERT(inst1 != NULL);
  wxASSERT(inst2 != NULL);

  if (inst1->VersionNumber() < inst2->VersionNumber())
    return true;

  int rank1 = inst1->GetType() == PgToolsRegistry::SYSTEM ? 0 : 1;
  int rank2 = inst2->GetType() == PgToolsRegistry::SYSTEM ? 0 : 1;
  if (rank1 < rank2)
    return true;

  return inst1->Path() < inst2->Path();
}

void PreferencesDialogue::OnInstallationSelected(wxListEvent& event)
{
  selectedInstallation = NULL;
  long installationIndex = installationsList->GetItemData(event.GetIndex());
  int index = 0;
  for (PgToolsRegistry::const_iterator iter = registry.begin(); iter != registry.end(); iter++, index++) {
    if (index == installationIndex) selectedInstallation = &(*iter);
  }

  removeInstallationButton->Enable(selectedInstallation != NULL && selectedInstallation->GetType() == PgToolsRegistry::USER);
}

void PreferencesDialogue::OnInstallationDeselected(wxListEvent& event)
{
  selectedInstallation = NULL;
  removeInstallationButton->Enable(false);
}

void PreferencesDialogue::OnRemoveInstallation(wxCommandEvent& event)
{
  wxASSERT(selectedInstallation != NULL);
  registry.remove(*selectedInstallation);
  removeInstallationButton->Enable(false);
  LoadInstallations();
}

void PreferencesDialogue::OnAddInstallation(wxCommandEvent& event)
{
  wxDirDialog dbox(this, _("Select Installation"));
  dbox.CentreOnParent();
  if (dbox.ShowModal() != wxID_CANCEL) {
    installationSuggestion = dbox.GetPath();
    ::wxGetApp().GetToolsRegistry().AddUserInstallation(installationSuggestion, this, XRCID("InstallationsList"));
    wxBeginBusyCursor();
  }
}

void PreferencesDialogue::OnToolsRegistryUpdated(PQWXToolsRegistryEvent& event)
{
  if (!installationSuggestion.empty()) {
    wxEndBusyCursor();
    if (event.discoveryResult.empty()) {
      wxLogError(_("No PostgreSQL installation was found at %s"), installationSuggestion.c_str());
    }
    else {
      std::vector<PgToolsRegistry::Installation> dupes;
      std::vector<PgToolsRegistry::Installation> added;
      for (std::vector<PgToolsRegistry::DiscoveryResult>::const_iterator iter = event.discoveryResult.begin(); iter != event.discoveryResult.end(); iter++) {
	switch ((*iter).action) {
	case PgToolsRegistry::DiscoveryResult::DUPLICATE:
	  dupes.push_back((*iter).installation);
	  break;
	case PgToolsRegistry::DiscoveryResult::ADDED:
	  added.push_back((*iter).installation);
	  break;
	}
      }
      if (added.empty()) {
	if (dupes.size() == 1) {
	  wxLogError(_("The PostgreSQL installation at %s was already added"), dupes[0].Path().c_str());
	}
	else {
	  wxLogError(_("All PostgreSQL installations found at %s were already added"), installationSuggestion.c_str());
	}
      }
      else {
	std::copy(added.begin(), added.end(), std::back_inserter(addedInstallations));
      }
    }
    installationSuggestion = wxEmptyString;
  }

  LoadInstallations();
}

void PreferencesDialogue::SaveInstallations()
{
  wxConfigBase *cfg = wxConfig::Get();
  wxString oldPath = cfg->GetPath();
  cfg->SetPath(_T("Installations"));

  int pos = 0;
  for (PgToolsRegistry::iterator iter = registry.begin(); iter != registry.end(); iter++) {
    if ((*iter).GetType() == PgToolsRegistry::SYSTEM)
      continue;
    wxString key = wxString::Format(_T("%d"), pos++);
    const wxString& path = (*iter).Path();
    cfg->Write(key + _T("/Location"), path);
  }

  do {
    wxString key = wxString::Format(_T("%d"), pos++);
    if (!cfg->Exists(key))
        break;
    wxLogDebug(_T("Pruning configuration group \"%s\""), key.c_str());
    if (!cfg->DeleteGroup(key))
      break;
  } while (1);

  cfg->SetPath(oldPath);
}

void PreferencesDialogue::CancelInstallations()
{
  for (std::vector<PgToolsRegistry::Installation>::const_iterator iter = addedInstallations.begin(); iter != addedInstallations.end(); iter++) {
    registry.remove(*iter);
  }
}
