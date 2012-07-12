/**
 * @file
 * Global IDs and declare the application class itself
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __nearby_servers_registry_h
#define __nearby_servers_registry_h

#include <list>

#include "avahi.h"

/**
 * Cache nearby database servers discovered with Avahi.
 */
class NearbyServersRegistry : public PQWXAvahi::Channel {
public:
  NearbyServersRegistry() : avahiClient(this, avahiPoller, AVAHI_CLIENT_NO_FAIL), avahiBrowser(NULL) {}
  ~NearbyServersRegistry() {}

  class ServerAddress {
  public:
    int family;
    wxString address;
    wxUint16 port;
  };

  class ServerInfo {
  public:
    wxString name;
    wxString hostname;
    bool local;
    std::vector<ServerAddress> addresses;
  };

  const std::list<ServerInfo>& GetDiscoveredServers() const { return servers; }

  void Start()
  {
    avahiPoller.Start();
    avahiBrowser = new PQWXAvahi::ServiceBrowser(avahiClient, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, _T("_postgresql._tcp"), wxEmptyString, (AvahiLookupFlags)0);
  }

  void Stop()
  {
    if (avahiBrowser != NULL) delete avahiBrowser;
    avahiPoller.Stop();
  }

  // AVAHI channel
  void ServiceFound(const wxString& name, const wxString& type, const wxString& domain, const wxString& hostName, bool local, int interface, int addressFamily, const wxString& addr, wxUint16 port);
  void ServiceLost(const wxString &name, const wxString& type, const wxString& domain);
  void ServiceAddressLost(const wxString& name, const wxString& type, const wxString& domain, const wxString& hostName, int interface, int addressFamily);
  void ServiceRefreshFinished(const wxString& type);

private:
  std::list<ServerInfo> servers;

  std::list<ServerInfo>::iterator FindServer(const wxString& name);

  PQWXAvahi::ThreadedPoller avahiPoller;
  PQWXAvahi::Client avahiClient;
  PQWXAvahi::ServiceBrowser *avahiBrowser;
};


#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
