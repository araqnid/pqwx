#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#ifndef __WXMSW__
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <stdexcept>
#include <algorithm>
#include "wx/dir.h"
#include "wx/tokenzr.h"
#include "wx/regex.h"
#include "pg_tools_registry.h"

#ifdef __WXMSW__
#define EXEC_SUFFIX ".exe"
#define PATH_SEPARATOR _T('\\')
#else
#define EXEC_SUFFIX ""
#define PATH_SEPARATOR _T('/')
#endif

DEFINE_LOCAL_EVENT_TYPE(PQWX_ToolsRegistryUpdated)

const std::vector<wxString>& PgToolsRegistry::ScannerThread::GetInterestingCommands()
{
  static std::vector<wxString> commands;
  if (commands.empty()) {
    // psql is assumed.
    commands.push_back(_T("pg_dump"));
    commands.push_back(_T("pg_restore"));
    commands.push_back(_T("postgres"));
  }
  return commands;
}

void PgToolsRegistry::AddInstallations(const std::vector<Suggestion> suggestions, bool includeSystemPath)
{
  wxCriticalSectionLocker locker(guardState);
  wxCHECK2(workerThread == NULL, );
  workerThread = new ScannerThread(this, suggestions, includeSystemPath, new MarkFinishedOnCompletion(this));
  workerThread->Create();
  workerThread->Run();
}

void PgToolsRegistry::AddUserInstallation(const wxString& path, wxEvtHandler *notify, int id)
{
  wxCriticalSectionLocker locker(guardState);
  wxCHECK2(workerThread == NULL, );
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion(USER, path));
  workerThread = new ScannerThread(this, suggestions, false, new NotifyWindowOnCompletion(notify, id));
  workerThread->Create();
  workerThread->Run();
}

bool PgToolsRegistry::IsScanningFinished() const
{
  wxCriticalSectionLocker locker(guardState);
  return finishedScanning;
}

void PgToolsRegistry::NotifyWindowOnCompletion::FinishedScanning(const std::vector<DiscoveryResult>& result)
{
  wxLogDebug(_T("Finished scanning for installations on behalf of %p"), dest);
  PQWXToolsRegistryEvent event(PQWX_ToolsRegistryUpdated, id);
  event.discoveryResult = result;
  dest->AddPendingEvent(event);
}

void PgToolsRegistry::ScannerThread::ScanForInstallations()
{
  for (std::vector<Suggestion>::const_iterator iter = suggestions.begin(); iter != suggestions.end(); iter++) {
    ScanSuggestedLocation((*iter).GetType(), (*iter).GetPath());
  }
  if (scanSystemPath)
    ScanExecutionPath();
}

int PgToolsRegistry::Installation::ParseVersion(const wxString& version)
{
  static wxRegEx tripletPattern(_T("^([0-9]+)\\.([0-9]+)\\.([0-9]+)$"));
  static wxRegEx dupletPattern(_T("^([0-9]+)\\.([0-9]+)(beta[0-9]+|devel)$"));
  if (tripletPattern.Matches(version)) {
    long majorVersion;
    long minorVersion;
    long patchVersion;
    tripletPattern.GetMatch(version, 1).ToLong(&majorVersion);
    tripletPattern.GetMatch(version, 2).ToLong(&minorVersion);
    tripletPattern.GetMatch(version, 3).ToLong(&patchVersion);
    return majorVersion * 10000 + minorVersion * 100 + patchVersion;
  }
  else if (dupletPattern.Matches(version)) {
    long majorVersion;
    long minorVersion;
    dupletPattern.GetMatch(version, 1).ToLong(&majorVersion);
    dupletPattern.GetMatch(version, 2).ToLong(&minorVersion);
    return majorVersion * 10000 + minorVersion * 100;
  }
  else {
    return -1;
  }
}

void PgToolsRegistry::ScannerThread::ScanSuggestedLocation(Type type, const wxString& dir)
{
  wxLogDebug(_T("Scanning suggested location: %s"), dir.c_str());

  const std::vector<wxString> firstLevelExecutables = FindExecutables(dir);
  if (Contains(firstLevelExecutables, _T("psql" EXEC_SUFFIX))) {
    AddInstallation(type, dir, dir);
    return;
  }

  const std::vector<wxString> firstLevelDirs = FindSubdirectories(dir);
  if (Contains(firstLevelDirs, _T("bin"))) {
    AddInstallation(type, dir, dir + PATH_SEPARATOR + _T("bin"));
    return;
  }

  for (std::vector<wxString>::const_iterator iter = firstLevelDirs.begin(); iter != firstLevelDirs.end(); iter++) {
    wxString firstLevelDir = dir + PATH_SEPARATOR + *iter;

    const std::vector<wxString> secondLevelExecutables = FindExecutables(firstLevelDir);
    if (Contains(secondLevelExecutables, _T("psql" EXEC_SUFFIX))) {
      AddInstallation(type, firstLevelDir, firstLevelDir);
      continue;
    }

    const std::vector<wxString> secondLevelDirs = FindSubdirectories(firstLevelDir);
    if (Contains(secondLevelDirs, _T("bin"))) {
      AddInstallation(type, firstLevelDir, firstLevelDir + PATH_SEPARATOR + _T("bin"));
    }
  }
}

void PgToolsRegistry::ScannerThread::ScanExecutionPath()
{
  wxLogDebug(_T("Scanning execution path"));

  wxString version = GetToolVersion(_T("psql" EXEC_SUFFIX), true);
  if (version.empty()) return;

  Installation installation(SYSTEM, wxEmptyString, wxEmptyString, version);
  installation.AddCommand(_T("psql"));
  wxLogDebug(_T(" Registered installation version %s (%u)"), installation.Version().c_str(), installation.VersionNumber());

  const std::vector<wxString>& interestingCommands = GetInterestingCommands();

  for (std::vector<wxString>::const_iterator iter = interestingCommands.begin(); iter != interestingCommands.end(); iter++) {
    if (GetToolVersion(*iter + _T(EXEC_SUFFIX), true) == version)
      installation.AddCommand(*iter);
  }

  owner->installations.push_back(installation);
}

void PgToolsRegistry::ScannerThread::AddInstallation(Type type, const wxString& dir, const wxString &bindir)
{
  wxLogDebug(_T("Checking installation in %s (commands in %s)"), dir.c_str(), bindir.c_str());

  std::vector<wxString> executables = FindExecutables(bindir);
  if (!Contains(executables, _T("psql" EXEC_SUFFIX))) {
    wxLogDebug(_T(" No psql"));
    return;
  }
  wxString version = GetToolVersion(bindir + PATH_SEPARATOR + _T("psql" EXEC_SUFFIX), false);
  if (version.empty()) {
    return;
  }

  Installation installation(type, dir, bindir, version);
  installation.AddCommand(_T("psql"));
  wxLogDebug(_T(" Registered installation version %s (%u)"), installation.Version().c_str(), installation.VersionNumber());

  const std::vector<wxString>& interestingCommands = GetInterestingCommands();

  for (std::vector<wxString>::const_iterator iter = interestingCommands.begin(); iter != interestingCommands.end(); iter++) {
    if (GetToolVersion(bindir + PATH_SEPARATOR + *iter + _T(EXEC_SUFFIX), false) == version)
      installation.AddCommand(*iter);
  }

  if (owner->RegisterInstallation(installation)) {
    result.push_back(DiscoveryResult(DiscoveryResult::ADDED, installation));
  }
  else {
    result.push_back(DiscoveryResult(DiscoveryResult::DUPLICATE, installation));
  }
}

std::vector<wxString> PgToolsRegistry::ScannerThread::FindSubdirectories(const wxString &dirname)
{
  wxDir dir(dirname);
  std::vector<wxString> result;
  if (!dir.IsOpened())
    return result;
  wxString filename;
  bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
  while (cont) {
    result.push_back(filename);
    cont = dir.GetNext(&filename);
  }

  return result;
}

std::vector<wxString> PgToolsRegistry::ScannerThread::FindExecutables(const wxString &dirname)
{
  wxDir dir(dirname);
  std::vector<wxString> result;
  if (!dir.IsOpened())
    return result;
  wxString filename;
#ifdef __WXMSW__
  bool cont = dir.GetFirst(&filename, _T("*.exe"), wxDIR_FILES);
#else
  bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES);
#endif
  for (; cont; cont = dir.GetNext(&filename)) {
#ifndef __WXMSW__
    struct stat statbuf;
    wxString fullName = dirname + _T('/') + filename;
    if (stat(fullName.utf8_str(), &statbuf) != 0)
      continue;
    if (access(fullName.utf8_str(), X_OK) != 0)
      continue;
#endif
    result.push_back(filename);
  }

  return result;
}

wxString PgToolsRegistry::ScannerThread::GetToolVersion(const wxString& toolExe, bool onPath)
{
  wxCharBuffer exeBuffer = toolExe.utf8_str();
  if (!onPath && access(exeBuffer.data(), X_OK) != 0) return wxEmptyString;

  std::vector<const char*> args;
  args.push_back(exeBuffer.data());
  args.push_back("-V");
  args.push_back(NULL);

  char buffer[1024];

#ifndef __WXMSW__
  int pipe[2];
  if (pipe2(pipe, 0) != 0) throw std::runtime_error("pipe failed");

  pid_t pid;
  if ((pid = fork()) < 0) throw std::runtime_error("fork failed");
  if (pid == 0) {
    // child process; redirect stdout to pipe
    if (dup2(pipe[1], 1) < 0) {
      exit(200);
    }
    if (onPath)
      execvp(exeBuffer.data(), (char* const*) &args[0]);
    else
      execv(exeBuffer.data(), (char* const*) &args[0]);
    exit(200);
  }

  // parent process
  close(pipe[1]);

  int got = read(pipe[0], buffer, sizeof(buffer));
  if (got < 0) throw std::runtime_error("read failed");
  unsigned rcvd = got;
  while (rcvd < sizeof(buffer)) {
    got = read(pipe[0], buffer + rcvd, sizeof(buffer) - rcvd);
    if (got < 0) throw std::runtime_error("read failed");
    if (got == 0) {
      break;
    }
  }
  wxString output(buffer, wxConvUTF8, rcvd);
  close(pipe[0]);

  int status;
  if (waitpid(pid, &status, 0) < 0) throw std::runtime_error("waitpid failed");

  if (WIFEXITED(status)) {
    int exitCode = WEXITSTATUS(status);
    if (exitCode > 0) {
      wxLogDebug(_T("Exited with status %d"), WEXITSTATUS(status));
      return wxEmptyString;
    }
    else {
      static wxRegEx pattern(_T("^([^ ]+) .+ ([0-9]+\\.[0-9]+(\\.[0-9]+|beta[0-9]+|devel))$"), wxRE_NEWLINE);
      if (pattern.Matches(output)) {
        wxString toolname = pattern.GetMatch(output, 1);
        wxString version = pattern.GetMatch(output, 2);
        wxLogDebug(_T("Found %s version %s at %s"), toolname.c_str(), version.c_str(), toolExe.c_str());
        return version;
      }
      else {
        wxLogDebug(_T("Output did not match pattern: %s"), output.c_str());
        return wxEmptyString;
      }
    }
  }
  else if (WIFSIGNALED(status)) {
    wxLogDebug(_T("Died with signal %d"), WTERMSIG(status));
    return wxEmptyString;
  }
  else {
    wxLogDebug(_T("Indecipherable wait status %x"), status);
    return wxEmptyString;
  }
#else
#warning "Finding external PostgreSQL client tools not implemented on Win32 yet"
  return wxEmptyString;
#endif
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
