// -*- c++ -*-

#ifndef __connect_dialogue_h
#define __connect_dialogue_h

#include <list>
#include "wx/xrc/xmlres.h"
#include "object_browser.h"

class ConnectionWork;

class ConnectDialogue : public wxDialog {
public:
  class CompletionCallback {
  public:
    virtual void Connected(const ServerConnection &server, DatabaseConnection *db) = 0;
    virtual void Cancelled() = 0;
  };

  ConnectDialogue(wxWindow *parent, CompletionCallback *callback)
    : wxDialog(), callback(callback), recentServersConfigPath(_T("RecentServers")) {
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

  void Suggest(const ServerConnection &conninfo);
  void DoInitialConnection(const ServerConnection &conninfo);

protected:
  wxComboBox *hostnameInput;
  wxComboBox *usernameInput;
  wxTextCtrl *passwordInput;
  wxCheckBox *savePasswordInput;
  wxButton *okButton;
  wxButton *cancelButton;

private:
  static const unsigned maxRecentServers = 10;
  CompletionCallback *callback;
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
    bool operator==(const RecentServerParameters &other) const {
      return server == other.server;
    }
    bool operator==(const wxString &otherName) const {
      return server == otherName;
    }
  };
  std::list<RecentServerParameters> recentServerList;
  const wxString recentServersConfigPath;
  // read/write just transfers recentServerList (model) to configuration file
  void ReadRecentServers();
  void WriteRecentServers();
  // load/save calls read/write and moves data from recentServerList to the UI and back
  void LoadRecentServers();
  void LoadRecentServer(const RecentServerParameters&);
  void SaveRecentServers();
  void SetupDefaults();

  DECLARE_EVENT_TABLE();
};

#endif
