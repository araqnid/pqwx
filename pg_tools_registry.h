/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __pg_tools_registry_h
#define __pg_tools_registry_h

#include <set>
#include <vector>
#include <list>
#include <algorithm>

BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(PQWX_ToolsRegistryUpdated, -1)
END_DECLARE_EVENT_TYPES()

/**
 * Remembers which PostgreSQL command-line tools are available in which locations.
 */
class PgToolsRegistry {
public:
  enum Type { SYSTEM, USER };

  class Suggestion {
  public:
    Suggestion(Type type, wxString path) : type(type), path(path) {}
    Type GetType() const { return type; }
    wxString GetPath() const { return path; }
  private:
    Type type;
    wxString path;
  };

  PgToolsRegistry() : workerThread(NULL) {}

  class Installation {
  public:
    Installation(Type type, const wxString &path, const wxString &commandPrefix, const wxString &version) : type(type), path(path), commandPrefix(commandPrefix), version(version), versionNumber(ParseVersion(version)) {}

    /**
     * Gets the full pathname of a particular command within this installation.
     */
    wxString GetCommandPath(const wxString &command) const;
    /**
     * This path describes the installation, and should be usable as a future suggestion to find this installation again.
     */
    wxString Path() const { return path; }
    /**
     * Gets the raw version number of this installation as a string.
     */
    wxString Version() const { return version; }
    Type GetType() const { return type; }
    /**
     * Gets the decoded version number of this installation.
     *
     * Should take the same form as PG_VERSION_NUM, e.g. "90103" for 9.1.3.
     */
    int VersionNumber() const { return versionNumber; }

    void AddCommand(const wxString &command) { commands.insert(command); }

    bool operator==(const Installation& other) const
    {
      return path == other.path;
    }
  private:
    Type type;
    wxString path;
    wxString commandPrefix;
    wxString version;
    int versionNumber;
    std::set<wxString> commands;
    static int ParseVersion(const wxString &version);
  };

  typedef std::list<Installation>::iterator iterator;
  typedef std::list<Installation>::const_iterator const_iterator;

  iterator begin() { return installations.begin(); }
  const_iterator begin() const { return installations.begin(); }
  iterator end() { return installations.end(); }
  const_iterator end() const { return installations.end(); }
  void remove(const Installation& installation) { installations.remove(installation); }

  void AddInstallations(const std::vector<Suggestion> suggestions, bool includeSystemPath);
  bool IsScanningFinished() const;
  void AddUserInstallation(const wxString& dir, wxEvtHandler* notify = NULL, int id = wxID_ANY);

  class DiscoveryResult {
  public:
    enum Action { ADDED, DUPLICATE };
    DiscoveryResult(Action action, const Installation& installation) : action(action), installation(installation) {}
    Action action;
    Installation installation;
  };

private:

  class CompletionCallback {
  public:
    virtual ~CompletionCallback() {}
    virtual void FinishedScanning(const std::vector<DiscoveryResult>& result) = 0;
  };

  class ScannerThread : public wxThread {
  public:
    ScannerThread(PgToolsRegistry *owner, const std::vector<Suggestion>& suggestions, bool includeSystemPath, CompletionCallback *completion = NULL) : wxThread(wxTHREAD_DETACHED), owner(owner), completion(completion), suggestions(suggestions), scanSystemPath(includeSystemPath) {}
    ~ScannerThread()
    {
      wxCriticalSectionLocker locker(owner->guardState);
      owner->workerThread = NULL;
    }
  protected:
    ExitCode Entry()
    {
      try {
        ScanForInstallations();
      } catch (std::exception &e) {
        wxLogDebug(_T("Scanning for installations crashed: %s"), e.what());
      } catch (...) {
        wxLogDebug(_T("Scanning for installations crashed"));
      }

      if (completion != NULL) {
        completion->FinishedScanning(result);
        delete completion;
      }

      return 0;
    }
  private:
    PgToolsRegistry * const owner;
    CompletionCallback * const completion;
    std::vector<Suggestion> suggestions;
    const bool scanSystemPath;
    void ScanForInstallations();
    void ScanSuggestedLocation(Type type, const wxString& dir);
    void ScanExecutionPath();
    void AddInstallation(Type type, const wxString& dir, const wxString& bindir);
    std::vector<DiscoveryResult> result;
    static wxString GetToolVersion(const wxString& exe, bool onPath);
    static std::vector<wxString> FindSubdirectories(const wxString& dir);
    static std::vector<wxString> FindExecutables(const wxString& dir);
    static const std::vector<wxString>& GetInterestingCommands();
    template <typename Container>
    bool Contains(const Container& sequence, const typename Container::value_type& key)
    {
      return find(sequence.begin(), sequence.end(), key) != sequence.end();
    }
  };

  std::list<Installation> installations;
  bool finishedScanning;
  mutable wxCriticalSection guardState;
  ScannerThread *workerThread;

  bool RegisterInstallation(const Installation& installation)
  {
    if (std::find(installations.begin(), installations.end(), installation) != installations.end())
      return false;
    installations.push_back(installation);
    return true;
  }

  class MarkFinishedOnCompletion : public CompletionCallback {
  public:
    MarkFinishedOnCompletion(PgToolsRegistry* owner) : owner(owner) {}
    void FinishedScanning(const std::vector<DiscoveryResult>& result)
    {
      wxCriticalSectionLocker locker(owner->guardState);
      owner->finishedScanning = true;
    }
  private:
    PgToolsRegistry* owner;
  };

  class NotifyWindowOnCompletion : public CompletionCallback {
  public:
    NotifyWindowOnCompletion(wxEvtHandler* dest, int id) : dest(dest), id(id) {}
    void FinishedScanning(const std::vector<DiscoveryResult>& result);
  private:
    wxEvtHandler *dest;
    int id;
  };
};

class PQWXToolsRegistryEvent : public wxNotifyEvent
{
public:
  PQWXToolsRegistryEvent(wxEventType type, int id = wxID_ANY) : wxNotifyEvent(type, id) {}
  PQWXToolsRegistryEvent* Clone() const { return new PQWXToolsRegistryEvent(*this); }
  std::vector<PgToolsRegistry::DiscoveryResult> discoveryResult;
};

typedef void (wxEvtHandler::*PQWXToolsRegistryEventFunction)(PQWXToolsRegistryEvent&);

#define EVT_TOOLS_REGISTRY(id, type, fn) \
    DECLARE_EVENT_TABLE_ENTRY( type, id, -1, \
    (wxObjectEventFunction) (wxEventFunction) (PQWXToolsRegistryEventFunction) (wxNotifyEventFunction) \
    wxStaticCastEvent( PQWXToolsRegistryEventFunction, & fn ), (wxObject *) NULL ),

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
