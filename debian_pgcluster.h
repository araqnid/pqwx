/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __debian_pgcluster_h
#define __debian_pgcluster_h

#include <vector>
#include "wx/string.h"

struct PgCluster {
  wxString version;
  wxString name;
  int port;
};

/**
 * Registry of PostgreSQL clusters (servers) on the local machine.
 */
class PgClusters {
public:
  PgClusters();

  const std::vector<PgCluster>& LocalClusters() { return clusters; }
  wxString DefaultCluster();
  wxString ParseCluster(const wxString&);

private:
  std::vector<PgCluster> clusters;

  wxString ReadConfigValue(const wxString& filename, const wxString& keyword);
  std::vector<PgCluster> VersionClusters(const wxString& version);
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
