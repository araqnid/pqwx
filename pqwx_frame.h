// -*- c++ -*-

#include "server_connection.h"
#include "object_browser.h"

class PqwxFrame: public wxFrame {
public:
  PqwxFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);
  void OnNew(wxCommandEvent& event);
  void OnOpen(wxCommandEvent& event);

  void AddServerConnection(ServerConnection *conn);
private:
  DECLARE_EVENT_TABLE();

  ObjectBrowser *objectBrowser;
};

// controls and menu commands
enum {
  Pqwx_Quit = wxID_EXIT,
  Pqwx_About = wxID_ABOUT,
  Pqwx_New = wxID_NEW,
  Pqwx_Open = wxID_OPEN,

  // local
  Pqwx_ObjectBrowser = 16384
};
