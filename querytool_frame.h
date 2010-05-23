class QueryToolFrame: public wxFrame {
public:
  QueryToolFrame(const wxString& title);

  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);
  void OnNew(wxCommandEvent& event);
  void OnOpen(wxCommandEvent& event);

private:
  DECLARE_EVENT_TABLE();
};

// controls and menu commands
enum {
  QueryTool_Quit = wxID_EXIT,
  QueryTool_About = wxID_ABOUT,
  QueryTool_New = wxID_NEW,
  QueryTool_Open = wxID_OPEN
};

