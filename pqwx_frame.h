// -*- c++ -*-

#include "object_browser.h"

class PqwxFrame: public wxFrame {
public:
  PqwxFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);

  void OnConnectObjectBrowser(wxCommandEvent& event);
  void OnDisconnectObjectBrowser(wxCommandEvent& event);
  void OnFindObject(wxCommandEvent& event);

  void OnCloseFrame(wxCloseEvent& event);

  ObjectBrowser *objectBrowser;
private:
  DECLARE_EVENT_TABLE();
};

// controls and menu commands
enum {
  Pqwx_Quit = wxID_EXIT,
  Pqwx_About = wxID_ABOUT,

  // local
  Pqwx_ObjectBrowser = 16384,
  Pqwx_ConnectObjectBrowser = 16385,
  Pqwx_DisconnectObjectBrowser = 16386,
};
