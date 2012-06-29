#include "sql_dictionary_file.h"
#include "wx/tokenzr.h"
#include "wx/log.h"
#include "wx/regex.h"
#include "wx/filesys.h"

SqlDictionaryFile::SqlDictionaryFile(const wxString& filename)
{
  wxLogDebug(_T("Loading '%s'"), filename.c_str());
  wxFileSystem fs;
  wxFSFile *fsfile = fs.OpenFile(filename);
  wxInputStream *stream = fsfile->GetStream();
  Tag tag;
  wxString sqlBuffer;
  int lineno = 0;
  do {
    ++lineno;
    wxString line = ReadLine(stream);
    if (line.StartsWith(_T("-- SQL :: "))) {
      StringSplitter splitter(line.Mid(10), _T(" :: "));
      if (!tag.name.empty()) {
	buffers.push_back(wxCharBuffer(CollapseSql(sqlBuffer).utf8_str()));
	AddSql(tag.name, buffers.back().data(), tag.majorVersion, tag.minorVersion);
      }
      wxString stmtName = splitter.GetNextToken();
      if (splitter.HasMoreTokens()) {
	wxStringTokenizer vtkz(splitter.GetNextToken(), _T("."));
	wxString majorVersionString = vtkz.GetNextToken();
	wxString minorVersionString = vtkz.GetNextToken();
	long majorVersion;
	long minorVersion;
	majorVersionString.ToLong(&majorVersion);
	minorVersionString.ToLong(&minorVersion);
	tag = Tag(stmtName, lineno, majorVersion, minorVersion);
      }
      else {
	tag = Tag(stmtName, lineno);
      }
      sqlBuffer.Clear();
      sqlBuffer << _T("/* ") << tag.name;
      if (tag.majorVersion > 0) {
	sqlBuffer << _T(" (version >= ") << tag.majorVersion << _T('.') << tag.minorVersion << _T(')');
      }
      sqlBuffer << _T(" */ ");
    }
    else if (!tag.name.empty()) {
      sqlBuffer << line << _T('\n');
    }
  } while (!stream->Eof());
  delete fsfile;
  if (!tag.name.empty())
    AddSql(tag.name, sqlBuffer.utf8_str(), tag.majorVersion, tag.minorVersion);
}

wxString SqlDictionaryFile::CollapseSql(const wxString& sql)
{
  static wxRegEx commentsRx(_T("-- (.+)"));
  wxString buf(sql);
  commentsRx.ReplaceAll(&buf, _T("/* \\1 */"));
  wxString out;
  bool eatWhitespace = true;
  for (size_t ptr = 0; ptr < buf.length(); ptr++) {
    if (iswspace(buf[ptr])) {
      if (!eatWhitespace) {
	out += _T(' ');
	eatWhitespace = true;
      }
    }
    else {
      out += buf[ptr];
      eatWhitespace = false;
    }
  }
  return out;
}

wxString SqlDictionaryFile::ReadLine(wxInputStream* stream)
{
  static char buf[8192];
  size_t pos = 0;
  char ch;
  do {
    ch = stream->GetC();
    if (stream->LastRead() == 0 || ch == '\n')
      break;
    buf[pos++] = ch;
  } while (pos < sizeof(buf));
  if (pos > 0 && buf[pos] == '\r') --pos;
  return wxString(buf, wxConvUTF8, pos);
}

const SqlDictionary& GetObjectBrowserSqlFile()
{
  static SqlDictionaryFile sqlDictionary(_T("object_browser.sql"));
  return sqlDictionary;
}

const SqlDictionary& GetObjectBrowserScriptsSqlFile()
{
  static SqlDictionaryFile sqlDictionary(_T("object_browser_scripts.sql"));
  return sqlDictionary;
}

const SqlDictionary& GetDependenciesViewFile()
{
  static SqlDictionaryFile sqlDictionary(_T("dependencies_view.sql"));
  return sqlDictionary;
}

const SqlDictionary& GetCreateDatabaseDialogueFile()
{
  static SqlDictionaryFile sqlDictionary(_T("create_database_dialogue.sql"));
  return sqlDictionary;
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
