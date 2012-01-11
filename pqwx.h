/**
 * @file
 * Global IDs and declare the application class itself
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __pqwx_h
#define __pqwx_h

#include "pqwx_config.h"
#include <vector>

/*
 * controls and menu commands
 */
enum {
  // local
  Pqwx_ObjectBrowser = 16384,
  Pqwx_ConnectObjectBrowser,
  Pqwx_DisconnectObjectBrowser,
  Pqwx_MainSplitter,
  Pqwx_EditorSplitter,
  Pqwx_DocumentsNotebook,
  Pqwx_ResultsNotebook,
  Pqwx_MessagesPage,
  Pqwx_MessagesDisplay,
  Pqwx_ObjectFinderResults,
};

class DatabaseNotificationMonitor;

/**
 * The wxApp implementation for PQWX.
 *
 * This deals with creating the initial frame, and executing actions based on the command line.
 */
class PQWXApp : public wxApp {
public:
  virtual bool OnInit();
  void OnInitCmdLine(wxCmdLineParser &parser);
  bool OnCmdLineParsed(wxCmdLineParser &parser);
  int OnExit();
#ifdef PQWX_NOTIFICATION_MONITOR
  static DatabaseNotificationMonitor* GetNotificationMonitor();
#endif
private:
  bool haveInitial;
  wxString initialServer;
  wxString initialUser;
  wxString initialPassword;
  wxString initialDatabase;
  std::vector<wxString> initialFiles;
#ifdef PQWX_NOTIFICATION_MONITOR
  static DatabaseNotificationMonitor *monitor;
#endif
};

#endif

// Local Variables:
// mode: c++
// End:
