// -*- c++ -*-

#ifndef __connect_dialogue_h
#define __connect_dialogue_h

#include "wx/xrc/xmlres.h"

#include "object_browser.h"

class ConnectDialogue : public wxDialog {
public:
  ConnectDialogue(wxWindow *parent, ObjectBrowser *objectBrowser) : wxDialog(), objectBrowser(objectBrowser) {
    InitXRC(parent);
  }

  void OnConnect(wxCommandEvent& event);
  void OnCancel(wxCommandEvent& event);

protected:
  wxComboBox* hostnameInput;
  wxComboBox* usernameInput;
  wxTextCtrl* passwordInput;
  wxCheckBox* savePasswordInput;

private:
  ObjectBrowser *objectBrowser;
  void InitXRC(wxWindow *parent) {
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("connect"));
    hostnameInput = XRCCTRL(*this, "hostname_value", wxComboBox);
    usernameInput = XRCCTRL(*this, "username_value", wxComboBox);
    passwordInput = XRCCTRL(*this, "password_value", wxTextCtrl);
    savePasswordInput = XRCCTRL(*this, "save_password_value", wxCheckBox);
  }
  DECLARE_EVENT_TABLE();
};

#endif
