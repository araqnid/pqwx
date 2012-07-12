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
  addrInfo.address = addr;
  addrInfo.port = port;

  ServerInfo *server = FindServer(name);
  if (server == NULL) {
    servers.push_back(ServerInfo());
    server = &(servers.back());
    server->name = name;
    server->hostname = hostName;
    server->local = local;
  }
  server->addresses.push_back(addrInfo);
}

void NearbyServersRegistry::ServiceLost(const wxString &name, const wxString& type, const wxString& domain)
{
  wxLogDebug(_T("LOST:  \"%s\""), name.c_str());
  for (std::list<ServerInfo>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    if ((*iter).name == name) {
      servers.erase(iter);
    }
  }
}

void NearbyServersRegistry::ServiceRefreshFinished(const wxString& type)
{
  wxLogDebug(_T("Finished refreshing \"%s\""), type.c_str());
}

NearbyServersRegistry::ServerInfo* NearbyServersRegistry::FindServer(const wxString& name)
{
  for (std::list<ServerInfo>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    if ((*iter).name == name)
      return &(*iter);
  }
  return NULL;
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
