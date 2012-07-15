/**
 * @file
 * Preferences dialogue
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __preferences_dialogue_h
#define __preferences_dialogue_h

#include "wx/dialog.h"

class PreferencesDialogue : public wxDialog {
public:
  PreferencesDialogue(wxWindow *parent);

private:
  PgToolsRegistry& registry;
  wxListCtrl *installationsList;
  wxButton *addInstallationButton;
  wxButton *removeInstallationButton;

  void InitXRC(wxWindow* parent);
  void OnFinishPreferences(wxCommandEvent&);
  void OnCancelPreferences(wxCommandEvent&);

  void LoadInstallations();
  void OnInstallationSelected(wxListEvent&);
  void OnInstallationDeselected(wxListEvent&);
  void OnRemoveInstallation(wxCommandEvent&);
  void OnAddInstallation(wxCommandEvent&);
  void OnToolsRegistryUpdated(PQWXToolsRegistryEvent&);
  void SaveInstallations();
  void CancelInstallations();

  wxString installationSuggestion;
  const PgToolsRegistry::Installation *selectedInstallation;

  static int wxCALLBACK CollateInstallations(long item1, long item2, long sortData);

  std::vector<PgToolsRegistry::Installation> addedInstallations;

  DECLARE_EVENT_TABLE();
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
