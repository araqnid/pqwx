// -*- mode: c++ -*-

#ifndef __pqwx_h
#define __pqwx_h

// controls and menu commands
enum {
  // local
  Pqwx_ObjectBrowser = 16384,
  Pqwx_ConnectObjectBrowser,
  Pqwx_DisconnectObjectBrowser,
  Pqwx_MainSplitter,
  Pqwx_EditorSplitter,
  Pqwx_ScriptsNotebook,
  Pqwx_ResultsNotebook,
  Pqwx_ResultsPage,
  Pqwx_MessagesPage,
  Pqwx_MessagesDisplay,
  Pqwx_PlanPage,
  Pqwx_ObjectFinderResults,
};

class PQWXApp : public wxApp {
public:
  virtual bool OnInit();
  void OnInitCmdLine(wxCmdLineParser &parser);
  bool OnCmdLineParsed(wxCmdLineParser &parser);
private:
  bool haveInitial;
  wxString initialServer;
  wxString initialUser;
  wxString initialPassword;
};

#endif
