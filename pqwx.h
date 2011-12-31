// -*- mode: c++ -*-

#ifndef __pqwx_h
#define __pqwx_h

// controls and menu commands
enum {
  // local
  Pqwx_ObjectBrowser = 16384,
  Pqwx_ConnectObjectBrowser = 16385,
  Pqwx_DisconnectObjectBrowser = 16386,
  Pqwx_MainSplitter = 16387,
  Pqwx_EditorSplitter = 16388,
  Pqwx_ScriptsNotebook = 16389,
  Pqwx_ResultsNotebook = 16390,
  Pqwx_ResultsPage = 16391,
  Pqwx_MessagesPage = 16392,
  Pqwx_PlanPage = 16393,
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
