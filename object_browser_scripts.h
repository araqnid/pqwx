/**
 * @file
 * Database interaction to produce schema scripts
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_scripts_h
#define __object_browser_scripts_h

#include "object_browser_work.h"
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
  ScriptWork(ObjectBrowser *view, const ObjectModelReference& targetRef, Mode mode, Output output) : ObjectBrowserWork(targetRef.DatabaseRef(), new ScriptComplete(this), READ_ONLY, ScriptWork::GetSqlDictionary()), view(view), targetRef(targetRef), mode(mode), output(output) {}

protected:
  typedef WxStringConcatenator OutputIterator;

  virtual void GenerateScript(OutputIterator output) = 0;

  ObjectBrowser* const view;
  const ObjectModelReference targetRef;
  const Mode mode;

  static std::map<wxChar, wxString> PrivilegeMap(const wxString &spec);

private:
  class ScriptComplete : public ObjectBrowserWork::CompletionCallback {
  public:
    ScriptComplete(ScriptWork *owner) : owner(owner) {}
    void OnCompletion()
    {
      owner->OnCompletion();
    }
  private:
    ScriptWork * const owner;
  };

  void DoManagedWork()
  {
    GenerateScript(WxStringConcatenator(script, _T(";\n\n")));
  }

  wxString script;

  void UpdateModel(ObjectBrowserModel& model) {}
  void UpdateView(ObjectBrowser& ob) {}
  void OnCompletion();

  static const SqlDictionary& GetSqlDictionary();

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
  DatabaseScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), dboid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate database script: %s"), this, ref.Identify().c_str());
  }
protected:
  void GenerateScript(OutputIterator output);
private:
  const Oid dboid;
  static std::map<wxChar, wxString> privilegeMap;
};

/**
 * Produce table scripts.
 */
class TableScriptWork : public ScriptWork {
public:
  TableScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), reloid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate table script: %s"), this, ref.Identify().c_str());
  }
private:
  const Oid reloid;
  static std::map<wxChar, wxString> privilegeMap;
  void GenerateForeignKey(OutputIterator output, const wxString& tableName, const wxString& srcColumns, const wxString& dstColumns, const QueryResults::Row& fkeyRow);
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce index scripts.
 */
class IndexScriptWork : public ScriptWork {
public:
  IndexScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), reloid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate table script: %s"), this, ref.Identify().c_str());
  }
private:
  const Oid reloid;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce schema scripts.
 */
class SchemaScriptWork : public ScriptWork {
public:
  SchemaScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), nspoid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate schema script: %s"), this, ref.Identify().c_str());
  }
private:
  const Oid nspoid;
  static std::map<wxChar, wxString> privilegeMap;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce view scripts.
 */
class ViewScriptWork : public ScriptWork {
public:
  ViewScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), reloid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate view script: %s"), this, ref.Identify().c_str());
  }
private:
  const Oid reloid;
  static std::map<wxChar, wxString> privilegeMap;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce sequence scripts.
 */
class SequenceScriptWork : public ScriptWork {
public:
  SequenceScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), reloid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate sequence script: %s"), this, ref.Identify().c_str());
  }
private:
  const Oid reloid;
  static std::map<wxChar, wxString> privilegeMap;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce function scripts.
 */
class FunctionScriptWork : public ScriptWork {
public:
  FunctionScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), procoid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate function script: %s"), this, ref.Identify().c_str());
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
    std::copy(types2.begin(), types2.end(),
              std::copy(types1.begin(), types1.end(),
                        std::inserter(typeSet, typeSet.begin()))
              );
    return FetchTypes(typeSet);
  }
  std::map<Oid, Typeinfo> FetchTypes(const std::set<Oid> &types);
  static void EscapeCode(const wxString &src, wxString& buf);
  static std::vector<bool> ReadIOModeArray(const QueryResults::Row &row, unsigned index);
};

/**
 * Produce text search dictionary scripts.
 */
class TextSearchDictionaryScriptWork : public ScriptWork {
public:
  TextSearchDictionaryScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), dictoid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate text search dictionary script: %s"), this, ref.Identify().c_str());
  }
private:
  Oid dictoid;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce text search parser scripts.
 */
class TextSearchParserScriptWork : public ScriptWork {
public:
  TextSearchParserScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), prsoid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate text search parser script: %s"), this, ref.Identify().c_str());
  }
private:
  Oid prsoid;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce text search template scripts.
 */
class TextSearchTemplateScriptWork : public ScriptWork {
public:
  TextSearchTemplateScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), tmploid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate text search template script: %s"), this, ref.Identify().c_str());
  }
private:
  Oid tmploid;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce text search configuration scripts.
 */
class TextSearchConfigurationScriptWork : public ScriptWork {
public:
  TextSearchConfigurationScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), cfgoid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate text search configuration script: %s"), this, ref.Identify().c_str());
  }
private:
  Oid cfgoid;
protected:
  void GenerateScript(OutputIterator output);
  void GenerateMappings(OutputIterator output);
  wxString qualifiedName;
  static const std::vector<wxString> tokenTypeAliases;
};

/**
 * Produce operator scripts.
 */
class OperatorScriptWork : public ScriptWork {
public:
  OperatorScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), oproid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate operator configuration script: %s"), this, ref.Identify().c_str());
  }
private:
  const Oid oproid;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce type scripts.
 */
class TypeScriptWork : public ScriptWork {
public:
  TypeScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.DatabaseRef(), mode, output), typoid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate type configuration script: %s"), this, ref.Identify().c_str());
  }
private:
  const Oid typoid;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce tablespace scripts.
 */
class TablespaceScriptWork : public ScriptWork {
public:
  TablespaceScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.ServerRef(), mode, output), spcoid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate tablespace configuration script: %s"), this, ref.Identify().c_str());
  }
private:
  Oid spcoid;
  static std::map<wxChar, wxString> privilegeMap;
protected:
  void GenerateScript(OutputIterator output);
};

/**
 * Produce role scripts.
 */
class RoleScriptWork : public ScriptWork {
public:
  RoleScriptWork(ObjectBrowser *view, const ObjectModelReference& ref, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view, ref.ServerRef(), mode, output), roloid(ref.GetOid())
  {
    wxLogDebug(_T("%p: work to generate role configuration script: %s"), this, ref.Identify().c_str());
  }
private:
  Oid roloid;
protected:
  void GenerateScript(OutputIterator output);
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
