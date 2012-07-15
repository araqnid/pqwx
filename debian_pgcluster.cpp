#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "debian_pgcluster.h"

#define PGCLUSTER_BINDIR "/usr/lib/postgresql"
#define PGCLUSTER_CONFDIR "/etc/postgresql"

#include "wx/regex.h"
#include <fstream>
#include <unistd.h>
#include <dirent.h>
#include "pg_config.h"
#include "pg_config_manual.h"

static wxRegEx clusterPattern(_T("([0-9]+\\.[0-9]+)/(.+)"));
static wxRegEx remoteClusterPattern(_T("([^:]+):([0-9]*)"));
static wxRegEx settingPattern(_T("^([a-z_]+) *= *(.+)"));
static wxRegEx versionPattern(_T("[0-9]+\\.[0-9]+"));

wxString PgClusters::ReadConfigValue(const wxString& filename, const wxString& keyword)
{
  std::ifstream input(filename.fn_str());
  if (!input.is_open()) {
    return wxEmptyString;
  }

  std::string line;
  while (input.good()) {
    getline(input, line);
    wxString recoded(line.c_str(), wxConvUTF8);
    if (settingPattern.Matches(recoded) && settingPattern.GetMatch(recoded, 1) == keyword) {
      wxString value = settingPattern.GetMatch(recoded, 2);

      int comment = value.Find(_T('#'));
      if (comment != wxNOT_FOUND) value = value.Left(comment);

      value = value.Trim();
      if (value[0] == _T('\'') && value[value.length() - 1] == _T('\'')) {
        wxString unquoted;
        for (size_t pos = 1; pos < (value.length() - 1); pos++) {
          if (value[pos] == _T('\'') && value[pos+1] == _T('\'')) {
            unquoted << _T('\'');
            pos++;
          }
          else {
            unquoted << value[pos];
          }
        }
        return unquoted;
      }
      else
        return value;
    }
  }

  return wxEmptyString;
}

wxString PgClusters::ParseCluster(const wxString &server)
{
  if (!clusterPattern.Matches(server))
    return server;
  wxString clusterName = clusterPattern.GetMatch(server, 2);

  if (remoteClusterPattern.Matches(clusterName)) {
    wxLogDebug(_T("%s looks like a remote cluster"), server.c_str());
    if (remoteClusterPattern.GetMatch(clusterName, 2).IsEmpty())
      return remoteClusterPattern.GetMatch(clusterName, 1);
    else
      return clusterName;
  }

  wxString localConfigFile = _T(PGCLUSTER_CONFDIR "/") + clusterPattern.GetMatch(server, 1) + _T("/") + clusterName + _T("/postgresql.conf");
  wxString portString = ReadConfigValue(localConfigFile, _T("port"));

  if (portString.IsEmpty()) {
    wxLogError(_("Failed to find a 'port' setting in %s"), localConfigFile.c_str());
    return wxEmptyString;
  }

  long port;
  if (!portString.ToLong(&port)) {
    wxLogError(_("Incomprehensible 'port' setting in %s: \"%s\""), localConfigFile.c_str(), portString.c_str());
    return wxEmptyString;
  }

  wxString unixSocketDirectory = ReadConfigValue(localConfigFile, _T("unix_socket_directory"));

  wxString localServer;
  if (!unixSocketDirectory.IsEmpty() && unixSocketDirectory != _T(DEFAULT_PGSOCKET_DIR)) {
    localServer << unixSocketDirectory;
  }
  localServer << _T(':');
  if (port != DEF_PGPORT)
    localServer << port;

  return localServer;
}

static std::vector<wxString> ClusterVersions() {
  std::vector<wxString> versions;
  wxLogDebug(_T("Scanning " PGCLUSTER_BINDIR " for versions"));
  DIR *dir = opendir(PGCLUSTER_BINDIR);
  if (dir != NULL) {
    size_t len = offsetof(struct dirent, d_name) + pathconf(PGCLUSTER_BINDIR, _PC_NAME_MAX) + 1;
    struct dirent *entryData = (struct dirent*) malloc(len);
    struct dirent *result;
    do {
      if (readdir_r(dir, entryData, &result) != 0) {
        wxLogSysError(_T("Unable to read " PGCLUSTER_BINDIR " directory"));
        break;
      }
      if (result == NULL)
        break;
      wxString filename(entryData->d_name, wxConvUTF8);
      if (versionPattern.Matches(filename)) {
        wxLogDebug(_T(" Found '%s'"), filename.c_str());
        versions.push_back(filename);
      }
      else {
        wxLogDebug(_T(" Ignored '%s'"), filename.c_str());
      }
    } while (1);
    closedir(dir);
    free(entryData);
  }
#ifdef __WXDEBUG__
  else {
    wxLogSysError(_T("Unable to read " PGCLUSTER_BINDIR " directory"));
  }
#endif
  return versions;
}

std::vector<PgCluster> PgClusters::VersionClusters(const wxString &version)
{
  std::vector<PgCluster> clusters;
  wxString versionDir = _T(PGCLUSTER_CONFDIR "/") + version + _T("/");
  DIR *dir = opendir(versionDir.fn_str());
  if (dir != NULL) {
    size_t len = offsetof(struct dirent, d_name) + pathconf(PGCLUSTER_BINDIR, _PC_NAME_MAX) + 1;
    struct dirent *entryData = (struct dirent*) malloc(len);
    struct dirent *result;
    do {
      if (readdir_r(dir, entryData, &result) != 0) {
        wxLogSysError(_T("Unable to read '%s' directory"), versionDir.c_str());
        break;
      }
      if (result == NULL)
        break;
      wxString filename(entryData->d_name, wxConvUTF8);
      wxString configFile = versionDir + filename + _T("/postgresql.conf");
      struct stat statbuf;
      if (stat(configFile.fn_str(), &statbuf) == 0) {
        PgCluster cluster;
        cluster.version = version;
        cluster.name = filename;
        wxString portString = ReadConfigValue(configFile, _T("port"));
        long port;
        if (!portString.ToLong(&port)) {
          wxLogError(_("Incomprehensible 'port' setting in %s: \"%s\""), configFile.c_str(), portString.c_str());
          continue;
        }
        cluster.port = (int) port;
        clusters.push_back(cluster);
      }
    } while (1);
    closedir(dir);
    free(entryData);
  }
  return clusters;
}

PgClusters::PgClusters()
{
  std::vector<wxString> versions = ClusterVersions();
  for (std::vector<wxString>::const_iterator iter = versions.begin(); iter != versions.end(); iter++) {
    wxLogDebug(_T("Finding clusters for version %s"), (*iter).c_str());
    std::vector<PgCluster> versionClusters = VersionClusters(*iter);
    for (std::vector<PgCluster>::const_iterator clusterIter = versionClusters.begin(); clusterIter != versionClusters.end(); clusterIter++) {
      wxLogDebug(_T(" Found %s on port %d"), (*clusterIter).version.c_str(), (*clusterIter).port);
      clusters.push_back(*clusterIter);
    }
  }
}

wxString PgClusters::DefaultCluster()
{
  // TODO check $HOME/.postgresqlrc
  // TODO check /etc/postgresql-common/user_clusters

  for (std::vector<PgCluster>::const_iterator iter = clusters.begin(); iter != clusters.end(); iter++) {
    if ((*iter).port == DEF_PGPORT) {
      wxLogDebug(_T("Using cluster %s/%s on default port"), (*iter).version.c_str(), (*iter).name.c_str());
      return (*iter).version + _T("/") + (*iter).name;
    }
  }

  if (!clusters.empty()) {
    wxLogDebug(_T("Using first cluster, %s/%s"), clusters[0].version.c_str(), clusters[0].name.c_str());
    return clusters[0].version + _T("/") + clusters[0].name;
  }

  return wxEmptyString;
}

