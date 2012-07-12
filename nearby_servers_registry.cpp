#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "nearby_servers_registry.h"

void NearbyServersRegistry::ServiceFound(const wxString& name, const wxString& type, const wxString& domain, const wxString& hostName, bool local, int interface, int addressFamily, const wxString& addr, wxUint16 port)
{
  wxLogDebug(_T("FOUND: \"%s\" on %s port %u, %s, interface=%d, addressFamily=%d"), name.c_str(), addr.c_str(), port, local ? _T("local") : _T("not-local"), interface, addressFamily);

  ServerAddress addrInfo;
  addrInfo.family = addressFamily;
  addrInfo.address = addr;
  addrInfo.port = port;

  std::list<ServerInfo>::iterator server = FindServer(name);
  if (server == servers.end()) {
    ServerInfo newServer;
    newServer.name = name;
    newServer.hostname = hostName;
    newServer.local = local;
    server = servers.insert(servers.end(), newServer);
  }
  (*server).addresses.push_back(addrInfo);
}

void NearbyServersRegistry::ServiceAddressLost(const wxString& name, const wxString& type, const wxString& domain, const wxString& hostName, int interface, int addressFamily)
{
  wxLogDebug(_T("LOST:  \"%s\" address family %d on interface %d"), name.c_str(), addressFamily, interface);
  std::list<ServerInfo>::iterator server = FindServer(name);
  if (server == servers.end()) return;
  std::vector<ServerAddress>& addresses = server->addresses;
  for (std::vector<ServerAddress>::iterator iter = addresses.begin(); iter != addresses.end(); iter++) {
    if ((*iter).family == addressFamily) {
      addresses.erase(iter);
    }
  }
  if (addresses.empty()) {
    wxLogDebug(_T("Pruning server with no remaining addresses"));
    servers.erase(server);
  }
}

void NearbyServersRegistry::ServiceLost(const wxString &name, const wxString& type, const wxString& domain)
{
  wxLogDebug(_T("LOST:  \"%s\""), name.c_str());
  std::list<ServerInfo>::iterator server = FindServer(name);
  if (server == servers.end()) return;
  servers.erase(server);
}

void NearbyServersRegistry::ServiceRefreshFinished(const wxString& type)
{
  wxLogDebug(_T("Finished refreshing \"%s\""), type.c_str());
}

std::list<NearbyServersRegistry::ServerInfo>::iterator NearbyServersRegistry::FindServer(const wxString& name)
{
  for (std::list<ServerInfo>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    if ((*iter).name == name)
      return iter;
  }
  return servers.end();
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
