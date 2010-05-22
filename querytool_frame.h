class QueryToolFrame: public wxFrame {
public:
  QueryToolFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& evet);

private:
  DECLARE_EVENT_TABLE();
};

// controls and menu commands
enum {
  QueryTool_Quit = wxID_EXIT,
  QueryTool_About = wxID_ABOUT
};

