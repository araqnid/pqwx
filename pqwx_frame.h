class PqwxFrame: public wxFrame {
public:
  PqwxFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);
  void OnNew(wxCommandEvent& event);
  void OnOpen(wxCommandEvent& event);

private:
  DECLARE_EVENT_TABLE();
};

// controls and menu commands
enum {
  Pqwx_Quit = wxID_EXIT,
  Pqwx_About = wxID_ABOUT,
  Pqwx_New = wxID_NEW,
  Pqwx_Open = wxID_OPEN
};

