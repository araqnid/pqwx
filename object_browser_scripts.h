/**
 * @file
 * Database interaction to produce schema scripts
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_scripts_h
#define __object_browser_scripts_h

#include "object_browser_database_work.h"
#include "pqwx_util.h"

/**
 * Generate SQL script for some database item.
 *
 * The SQL script is generated in some mode (create, alter, etc) and
 * with a designated output. This class deals with dispatching
 * generated SQL to the output: subclasses must supply implementations
 * to produce the script by populating the statements member in an
 * Execute() implementation.
 */
class ScriptWork : public ObjectBrowserWork {
public:
  /**
   * Type of script to produce.
   */
  enum Mode { Create, Alter, Drop, Select, Insert, Update, Delete };
  /**
   * Output channel.
   */
  enum Output { Window, File, Clipboard };
  ScriptWork(const ServerConnection &server, const wxString &dbname, Mode mode, Output output) : ObjectBrowserWork(ScriptWork::GetSqlDictionary()), server(server), dbname(dbname), mode(mode), output(output) {}

protected:
  typedef WxStringConcatenator OutputIterator;

  virtual void GenerateScript(OutputIterator output) = 0;

  const ServerConnection& server;
  wxString dbname;
  const Mode mode;

  static std::map<wxChar, wxString> PrivilegeMap(const wxString &spec);

private:
  void operator()()
  {
    GenerateScript(WxStringConcatenator(script, _T(";\n\n")));
  }

  wxString script;

  void LoadIntoView(ObjectBrowser *ob)
  {
    wxString message;
    switch (output) {
    case Window: {
      PQWXDatabaseEvent evt(server, dbname, PQWX_ScriptToWindow);
      evt.SetString(script);
      ob->ProcessEvent(evt);
      return;
    }
      break;
    case File:
      message << _T("TODO Send to file:\n\n");
      break;
    case Clipboard:
      message << _T("TODO Send to clipboard:\n\n");
      break;
    default:
      wxASSERT(false);
    }
    wxLogDebug(_T("%s"), script.c_str());
    wxMessageBox(script);
  }

  static const VersionedSql& GetSqlDictionary();

public:
  static std::vector<Oid> ParseOidVector(const wxString &str);
  static std::vector<Oid> ReadOidVector(const QueryResults::Row &row, unsigned index) { return ParseOidVector(row[index]); }
  template <class OutputIterator>
  static void ParseTextArray(OutputIterator output, const wxString &str)
  {
    if (str.empty()) return;
    wxASSERT(str[0] == _T('{') && str[str.length()-1] == _T('}'));
    int state = 0;
    wxString buf;
    for (unsigned pos = 1; pos < str.length(); pos++) {
      wxChar c = str[pos];
      switch (state) {
      case 0:
	switch (c) {
	case _T('}'):
	  *output++ = buf;
	  return;

	case _T(','):
	  *output++ = buf;
	  buf.Clear();
	  break;

	case _T('\"'):
	  state = 1;
	  break;

	default:
	  buf << c;
	  break;
	}
	break;

      case 1:
	switch (c) {
	case _T('\"'):
	  state = 0;
	  break;

	case _T('\\'):
	  state = 2;
	  break;

	default:
	  buf << c;
	  break;
	}
	break;

      case 2:
	buf << c;
	state = 1;
	break;

      }
    }
  }
  static std::vector<wxString> ReadTextArray(const QueryResults::Row &row, unsigned index)
  {
    std::vector<wxString> result;
    ParseTextArray(std::back_inserter(result), row[index]);
    return result;
  }
  static std::vector<Oid> ReadOidArray(const QueryResults::Row &row, unsigned index)
  {
    std::vector<wxString> strings = ReadTextArray(row, index);
    std::vector<Oid> result;
    result.reserve(strings.size());
    for (std::vector<wxString>::const_iterator iter = strings.begin(); iter != strings.end(); iter++) {
      long value;
      if ((*iter).ToLong(&value)) {
	result.push_back((Oid) value);
      }
    }
    return result;
  }
  template <typename OutputIterator>
  void AddDescription(OutputIterator output, const wxString& ddlElement, const wxString& name, const wxString& description)
  {
    if (!description.empty())
      *output++ = _T("COMMENT ON ") + ddlElement + _T(' ') + name + _T(" IS ") + QuoteLiteral(description);
  }

private:
  const Output output;
};

/**
 * Produce database scripts.
 */
class DatabaseScriptWork : public ScriptWork {
public:
  DatabaseScriptWork(const ServerConnection& server, const wxString &dbname, Oid dboid, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(server, dbname, mode, output), dboid(dboid)
  {
    wxLogDebug(_T("%p: work to generate database script"), this);
  }
protected:
  void GenerateScript(OutputIterator output);
private:
  Oid dboid;
  static std::map<wxChar, wxString> privilegeMap;
};

/**
 * Produce table scripts.
 */
class TableScriptWork : public ScriptWork {
public:
  TableScriptWork(const ServerConnection& server, const wxString &dbname, Oid reloid, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(server, dbname, mode, output), reloid(reloid)
  {
    wxLogDebug(_T("%p: work to generate table script"), this);
  }
private:
  Oid reloid;
  static std::map<wxChar, wxString> privilegeMap;
  void GenerateForeignKey(OutputIterator output, const wxString& tableName, const wxString& keyName, const wxString& srcColumns, const wxString& dstColumns, const QueryResults::Row& fkeyRow);
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce view scripts.
 */
class ViewScriptWork : public ScriptWork {
public:
  ViewScriptWork(const ServerConnection& server, const wxString &dbname, Oid reloid, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(server, dbname, mode, output), reloid(reloid)
  {
    wxLogDebug(_T("%p: work to generate view script"), this);
  }
private:
  Oid reloid;
  static std::map<wxChar, wxString> privilegeMap;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce sequence scripts.
 */
class SequenceScriptWork : public ScriptWork {
public:
  SequenceScriptWork(const ServerConnection& server, const wxString &dbname, Oid reloid, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(server, dbname, mode, output), reloid(reloid)
  {
    wxLogDebug(_T("%p: work to generate sequence script"), this);
  }
private:
  Oid reloid;
  static std::map<wxChar, wxString> privilegeMap;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce function scripts.
 */
class FunctionScriptWork : public ScriptWork {
public:
  FunctionScriptWork(const ServerConnection& server, const wxString &dbname, Oid procoid, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(server, dbname, mode, output), procoid(procoid)
  {
    wxLogDebug(_T("%p: work to generate function script"), this);
  }
private:
  Oid procoid;
  static std::map<wxChar, wxString> privilegeMap;
protected:
  void GenerateScript(OutputIterator output);
private:
  struct Typeinfo {
    wxString schema;
    wxString name;
    int arrayDepth;
  };
  std::map<Oid, Typeinfo> FetchTypes(const std::vector<Oid> &types1, const std::vector<Oid> &types2) {
    std::set<Oid> typeSet;
    copy(types2.begin(), types2.end(),
	 copy(types1.begin(), types1.end(),
	      std::inserter(typeSet, typeSet.begin()))
	 );
    return FetchTypes(typeSet);
  }
  std::map<Oid, Typeinfo> FetchTypes(const std::set<Oid> &types);
  static void EscapeCode(const wxString &src, wxString& buf);
  static std::vector<bool> ReadIOModeArray(const QueryResults::Row &row, unsigned index);
};

#endif

// Local Variables:
// mode: c++
// End:
