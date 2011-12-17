// -*- c++ -*-

#ifndef __connect_dialogue_h
#define __connect_dialogue_h

#include <list>
#include "wx/xrc/xmlres.h"
#include "object_browser.h"

class ConnectionWork;

class ConnectDialogue : public wxDialog {
public:
  ConnectDialogue(wxWindow *parent, ObjectBrowser *objectBrowser) : wxDialog(), objectBrowser(objectBrowser),
								    recentServersConfigPath(_T("RecentServers")) {
    InitXRC(parent);
    LoadRecentServers();
    connection = NULL;
    cancelling = false;
  }

  void OnConnect(wxCommandEvent& event) {
    StartConnection();
  }
  void OnCancel(wxCommandEvent& event);
  void OnRecentServerChosen(wxCommandEvent& event);
  void OnConnectionFinished(wxCommandEvent& event);

  void DoInitialConnection(const wxString& server, const wxString& user, const wxString& password);

protected:
  wxComboBox *hostnameInput;
  wxComboBox *usernameInput;
  wxTextCtrl *passwordInput;
  wxCheckBox *savePasswordInput;
  wxButton *okButton;
  wxButton *cancelButton;

private:
  static const int maxRecentServers = 10;
  ObjectBrowser *objectBrowser;
  void InitXRC(wxWindow *parent) {
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("connect"));
    hostnameInput = XRCCTRL(*this, "hostname_value", wxComboBox);
    usernameInput = XRCCTRL(*this, "username_value", wxComboBox);
    passwordInput = XRCCTRL(*this, "password_value", wxTextCtrl);
    savePasswordInput = XRCCTRL(*this, "save_password_value", wxCheckBox);
    okButton = XRCCTRL(*this, "wxID_OK", wxButton);
    cancelButton = XRCCTRL(*this, "wxID_CANCEL", wxButton);
  }
  void StartConnection();
  void MarkBusy();
  void UnmarkBusy();
  ConnectionWork *connection;
  bool cancelling;

  class RecentServerParameters {
    wxString server;
    wxString username;
    wxString password;
    friend class ConnectDialogue;
  public:
    bool operator==(const RecentServerParameters &other) {
      return server == other.server;
    }
  };
  list<RecentServerParameters> recentServerList;
  const wxString recentServersConfigPath;
  // read/write just transfers recentServerList (model) to configuration file
  void ReadRecentServers();
  void WriteRecentServers();
  // load/save calls read/write and moves data from recentServerList to the UI and back
  void LoadRecentServers();
  void LoadRecentServer(const RecentServerParameters&);
  void SaveRecentServers();

  DECLARE_EVENT_TABLE();
};

#endif
