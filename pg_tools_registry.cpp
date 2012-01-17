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

int PgToolsRegistry::Installation::ParseVersion(const wxString& version)
{
  wxStringTokenizer tkz(version, _T("."));
  int versionNumber = 0;
  while (tkz.HasMoreTokens()) {
    long nextNumber;
    tkz.GetNextToken().ToLong(&nextNumber);
    versionNumber = versionNumber * 100 + (int) nextNumber;
  }
  return versionNumber;
}

template <typename T>
bool Contains(std::vector<T> const& sequence, T const key)
{
  return find(sequence.begin(), sequence.end(), key) != sequence.end();
}

void PgToolsRegistry::ScannerThread::ScanSuggestedLocation(const wxString& dir)
{
  wxLogDebug(_T("Scanning suggested location: %s"), dir.c_str());

  std::vector<wxString> firstLevelExecutables = FindExecutables(dir);
  if (Contains<wxString>(firstLevelExecutables, _T("psql" EXEC_SUFFIX))) {
    AddInstallation(dir);
    return;
  }

  std::vector<wxString> firstLevelDirs = FindSubdirectories(dir);
  if (Contains<wxString>(firstLevelDirs, _T("bin"))) {
    AddInstallation(dir + PATH_SEPARATOR + _T("bin"));
    return;
  }

  for (std::vector<wxString>::const_iterator iter = firstLevelDirs.begin(); iter != firstLevelDirs.end(); iter++) {
    wxString firstLevelDir = dir + PATH_SEPARATOR + *iter;

    std::vector<wxString> secondLevelExecutables = FindExecutables(firstLevelDir);
    if (Contains<wxString>(secondLevelExecutables, _T("psql" EXEC_SUFFIX))) {
      AddInstallation(firstLevelDir);
      continue;
    }

    std::vector<wxString> secondLevelDirs = FindSubdirectories(firstLevelDir);
    if (Contains<wxString>(secondLevelDirs, _T("bin"))) {
      AddInstallation(firstLevelDir + PATH_SEPARATOR + _T("bin"));
    }
  }
}

void PgToolsRegistry::ScannerThread::ScanExecutionPath()
{
  wxLogDebug(_T("Scanning execution path"));

  wxString version = GetToolVersion(_T("psql" EXEC_SUFFIX), true);
  if (version.empty()) return;

  Installation installation(wxEmptyString, version);
  installation.AddCommand(_T("psql"));

  const std::vector<wxString>& interestingCommands = GetInterestingCommands();

  for (std::vector<wxString>::const_iterator iter = interestingCommands.begin(); iter != interestingCommands.end(); iter++) {
    if (GetToolVersion(*iter + _T(EXEC_SUFFIX), true) == version)
      installation.AddCommand(*iter);
  }

  owner->installations.push_back(installation);
}

void PgToolsRegistry::ScannerThread::AddInstallation(const wxString &dirname)
{
  wxLogDebug(_T("Checking installation in %s"), dirname.c_str());

  std::vector<wxString> executables = FindExecutables(dirname);
  if (!Contains<wxString>(executables, _T("psql" EXEC_SUFFIX))) {
    wxLogDebug(_T(" No psql"));
    return;
  }
  wxString version = GetToolVersion(dirname + PATH_SEPARATOR + _T("psql" EXEC_SUFFIX), false);
  if (version.empty()) {
    return;
  }

  Installation installation(dirname, version);
  installation.AddCommand(_T("psql"));

  const std::vector<wxString>& interestingCommands = GetInterestingCommands();

  for (std::vector<wxString>::const_iterator iter = interestingCommands.begin(); iter != interestingCommands.end(); iter++) {
    if (GetToolVersion(dirname + PATH_SEPARATOR + *iter + _T(EXEC_SUFFIX), false) == version)
      installation.AddCommand(*iter);
  }

  owner->installations.push_back(installation);
}

std::vector<wxString> PgToolsRegistry::ScannerThread::FindSubdirectories(const wxString &dirname)
{
  wxDir dir(dirname);
  std::vector<wxString> result;
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
  wxString filename;
#ifdef __WXMSW__
  bool cont = dir.GetFirst(&filename, _T("*.exe"), wxDIR_FILES);
#else
  bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES);
#endif
  while (cont) {
#ifndef __WXMSW__
    struct stat statbuf;
    wxString fullName = dirname + _T('/') + filename;
    if (stat(fullName.utf8_str(), &statbuf) != 0)
      continue;
    if (access(fullName.utf8_str(), X_OK) != 0)
      continue;
#endif
    result.push_back(filename);
    cont = dir.GetNext(&filename);
  }

  return result;
}

wxString PgToolsRegistry::ScannerThread::GetToolVersion(const wxString& toolExe, bool onPath)
{
  wxCharBuffer exeBuffer = toolExe.utf8_str();
  if (!onPath && access(exeBuffer.data(), X_OK) != 0) return wxEmptyString;

  std::vector<char*> args;
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
      execvp(exeBuffer.data(), &args[0]);
    else
      execv(exeBuffer.data(), &args[0]);
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
      static wxRegEx pattern(_T("^([^ ]+) .+ ([0-9]+\\.[0-9]+\\.[0-9]+)$"), wxRE_NEWLINE);
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
