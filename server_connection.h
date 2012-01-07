/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __db__connection_manager_h
#define __db__connection_manager_h

#include <vector>
#include <string>
#include <map>

#include "wx/intl.h"
#include "pg_config.h"

/**
 * Defines connection parameters for a server.
 *
 * This now-horribly-misnamed class contains the requisite information
 * to make an initial connection to a server, including a suggested
 * "administrative" database name. It may also be updated with some
 * discovered attributes after connecting, for example to note if a
 * password is required to connect, server version or server-level
 * capabilities.
 */
class ServerConnection {
public:
  ServerConnection() : globalDbName(_T("postgres")) {
    port = -1;
    GenerateIdentification(wxEmptyString);
  }

  // connection parameters
  wxString identifiedAs;
  wxString hostname;
  int port;
  wxString username;
  wxString password;
  wxString globalDbName;

  // discovered attributes
  bool passwordNeededToConnect;

  void SetServerName(const wxString& serverName, const wxString& identifiedAs_ = wxEmptyString) {
    int colon = serverName.Find(_T(':'));
    if (colon == wxNOT_FOUND) {
      hostname = serverName;
    }
    else {
      if (colon > 0)
	hostname = serverName.Mid(0, colon);
      unsigned long portUL;
      serverName.Mid(colon+1).ToULong(&portUL);
      if (portUL == DEF_PGPORT)
	port = 0;
      else
	port = portUL;
    }

    identifiedAs = identifiedAs_;
    GenerateIdentification(identifiedAs);
  }

  const wxString& Identification() const {
    return identification;
  }

private:
  void GenerateIdentification(const wxString &identifiedAs) {
    identification.Clear();
    if (!username.IsEmpty()) {
      identification << username << _T('@');
    }
    if (!identifiedAs.IsEmpty()) {
      identification << identifiedAs;
    }
    else {
      if (!hostname.IsEmpty()) {
	identification << hostname;
      }
      else {
	identification << _("[local]");
      }
      if (port > 0) {
	identification << _T(":") << wxString::Format(_T("%d"), port);
      }
    }
  }

  wxString identification;
};

#endif

// Local Variables:
// mode: c++
// End:
