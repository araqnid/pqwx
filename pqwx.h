/**
 * @file
 * Global IDs and declare the application class itself
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __pqwx_h
#define __pqwx_h

#include "pqwx_config.h"
#include <vector>

#include "wx/app.h"
#include "pg_tools_registry.h"
#include "nearby_servers_registry.h"
#include "database_notification_monitor.h"

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

class ObjectBrowserModel;

/**
 * The wxApp implementation for PQWX.
 *
 * This deals with creating the initial frame, and executing actions based on the command line.
 */
class PQWXApp : public wxApp {
public:
#ifdef PQWX_NOTIFICATION_MONITOR
  PQWXApp() : monitor(NULL), objectBrowserModel(NULL) {}
  DatabaseNotificationMonitor& GetNotificationMonitor();
#else
  PQWXApp() : objectBrowserModel(NULL) {}
#endif
  ObjectBrowserModel& GetObjectBrowserModel() { return *objectBrowserModel; }
  PgToolsRegistry& GetToolsRegistry() { return toolsRegistry; }
  NearbyServersRegistry& GetNearbyServersRegistry() { return nearbyServers; }
private:
  bool haveInitial;
  wxString initialServer;
  wxString initialUser;
  wxString initialPassword;
  wxString initialDatabase;
  std::vector<wxString> initialFiles;
#ifdef PQWX_NOTIFICATION_MONITOR
  DatabaseNotificationMonitor *monitor;
#endif
  ObjectBrowserModel *objectBrowserModel;
  PgToolsRegistry toolsRegistry;
  NearbyServersRegistry nearbyServers;
  bool OnInit();
  int OnExit();
  void OnInitCmdLine(wxCmdLineParser &parser);
  bool OnCmdLineParsed(wxCmdLineParser &parser);
  void SuggestConfiguredToolLocations(std::vector<PgToolsRegistry::Suggestion>&);

  // AVAHI channel
  void ServiceFound(const wxString& name, const wxString& type, const wxString& domain, const wxString& hostName, bool local, int interface, int addressFamily, const wxString& addr, wxUint16 port);
  void ServiceLost(const wxString &name, const wxString& type, const wxString& domain);
  void ServiceRefreshFinished(const wxString& type);
};

DECLARE_APP(PQWXApp);

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
