/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __pg_tools_registry_h
#define __pg_tools_registry_h

#include <set>
#include <vector>
#include <list>

/**
 * Remembers which PostgreSQL command-line tools are available in which locations.
 */
class PgToolsRegistry {
public:
  PgToolsRegistry() : includeSystemPath(true), workerThread(NULL) {}

  class Installation {
  public:
    Installation(const wxString &path, const wxString &version) : pathname(path), version(version), versionNumber(ParseVersion(version)) {}

    wxString GetCommandPath(const wxString &command) const;
    wxString Version() const { return version; }
    int VersionNumber() const { return versionNumber; }

    void AddCommand(const wxString &command) { commands.insert(command); }
  private:
    wxString pathname;
    wxString version;
    int versionNumber;
    std::set<wxString> commands;
    static int ParseVersion(const wxString &version);
  };

  void AddSuggestion(const wxString& dir) { suggestions.push_back(dir); }
  void SetUseSystemPath(bool useSystemPath) { includeSystemPath = useSystemPath; }

  void BeginFindInstallations()
  {
    wxCriticalSectionLocker locker(guardState);
    wxCHECK2(workerThread == NULL, );
    workerThread = new ScannerThread(this);
    workerThread->Create();
    workerThread->Run();
  }
  bool IsScanningFinished() const
  {
    wxCriticalSectionLocker locker(guardState);
    return finishedScanning;
  }
  const std::list<Installation>& GetInstallations() const { return installations; }

private:
  class ScannerThread : public wxThread {
  public:
    ScannerThread(PgToolsRegistry *owner) : wxThread(wxTHREAD_DETACHED), owner(owner) {}
    ~ScannerThread()
    {
      wxCriticalSectionLocker locker(owner->guardState);
      owner->workerThread = NULL;
    }
  protected:
    ExitCode Entry()
    {
      ScanForInstallations();
      wxCriticalSectionLocker locker(owner->guardState);
      owner->finishedScanning = true;
      return 0;
    }
  private:
    PgToolsRegistry *owner;
    void ScanForInstallations()
    {
      for (std::vector<wxString>::const_iterator iter = owner->suggestions.begin(); iter != owner->suggestions.end(); iter++) {
        ScanSuggestedLocation(*iter);
      }
      if (owner->includeSystemPath)
        ScanExecutionPath();
    }
    void ScanSuggestedLocation(const wxString& dir);
    void ScanExecutionPath();
    void AddInstallation(const wxString& dir);
    static wxString GetToolVersion(const wxString& exe, bool onPath);
    static std::vector<wxString> FindSubdirectories(const wxString& dir);
    static std::vector<wxString> FindExecutables(const wxString& dir);
    static const std::vector<wxString>& GetInterestingCommands();
  };

  std::list<Installation> installations;
  std::vector<wxString> suggestions;
  bool includeSystemPath;
  bool finishedScanning;
  mutable wxCriticalSection guardState;
  ScannerThread *workerThread;

  friend class ScannerThread;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
