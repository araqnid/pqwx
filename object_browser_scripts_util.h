/**
 * @file
 * Utilities used to implement object browser script generators
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_scripts_util_h
#define __object_browser_scripts_util_h

#include "wx/tokenzr.h"
#include "wx/regex.h"

class PgAcl {
public:
  PgAcl(const wxString& input)
  {
    if (!input.empty()) {
      wxASSERT(input[0] == _T('{'));
      wxASSERT(input[input.length()-1] == _T('}'));
      wxStringTokenizer tkz(input.Mid(1, input.length()-2), _T(","));
      while (tkz.HasMoreTokens()) {
        entries.push_back(AclEntry(tkz.GetNextToken()));
      }
    }
  }

  template <class OutputIterator>
  void GenerateGrantStatements(OutputIterator output, const wxString &owner, const wxString &entity, const std::map<wxChar, wxString> &privilegeNames) const
  {
    for (std::vector<AclEntry>::const_iterator iter = entries.begin(); iter != entries.end(); iter++) {
      if ((*iter).grantee != owner) {
        (*iter).GenerateGrantStatement(output, entity, privilegeNames, true);
        (*iter).GenerateGrantStatement(output, entity, privilegeNames, false);
      }
    }
  }

  class InvalidAclEntry : public std::exception {
  public:
    InvalidAclEntry(const wxString &input) : input(input) {}
    ~InvalidAclEntry() throw () {}
    const char *what() { return "Invalid ACL entry"; }
    const wxString& GetInput() const { return input; }
  private:
    wxString input;
  };

private:
  class AclEntry {
  public:
    AclEntry(const wxString &input)
    {
      if (!pattern.Matches(input)) throw InvalidAclEntry(input);
      grantee = pattern.GetMatch(input, 1);
      grantor = pattern.GetMatch(input, 3);
      wxString privs = pattern.GetMatch(input, 2);
      for (unsigned i = 0; i < privs.length(); i++) {
        Privilege priv;
        priv.specifier = privs[i];
        if (i < (privs.length()-1) && privs[i+1] == '*') {
          priv.grantOption = true;
          ++i;
        }
        else {
          priv.grantOption = false;
        }
        privileges.push_back(priv);
      }
    }

    wxString grantee;
    struct Privilege {
      wxChar specifier;
      bool grantOption;
    };
    std::vector<Privilege> privileges;
    wxString grantor;

    template <class OutputIterator>
    void GenerateGrantStatement(OutputIterator output, const wxString& entity, const std::map<wxChar, wxString> &privilegeNames, bool withGrantOption) const
    {
      wxString sql = _T("GRANT ");
      bool first = true;
      for (std::vector<Privilege>::const_iterator iter = privileges.begin(); iter != privileges.end(); iter++) {
        if ((*iter).grantOption != withGrantOption)
          continue;
        std::map<wxChar, wxString>::const_iterator privPtr = privilegeNames.find((*iter).specifier);
        wxASSERT(privPtr != privilegeNames.end());
        if (first)
          first = false;
        else
          sql << _T(", ");
        sql << privPtr->second;
      }
      if (first) return;
      sql << _T(" ON ") << entity << _T(" TO ");
      if (grantee.empty())
        sql << _T("PUBLIC");
      else
        sql << grantee;
      if (withGrantOption)
        sql << _T(" WITH GRANT OPTION");
      *output++ = sql;
    }

  private:
    static wxRegEx pattern;
  };

  std::vector<AclEntry> entries;
};

class PgSettings {
public:
  PgSettings(const wxString &input)
  {
    ScriptWork::ParseTextArray(std::back_inserter(settings), input);
  }

  template <class OutputIterator>
  void GenerateSetStatements(OutputIterator output, ObjectBrowserWork *work, const wxString &prefix = wxEmptyString, bool brackets = false) const
  {
    for (std::vector<Setting>::const_iterator iter = settings.begin(); iter != settings.end(); iter++) {
      (*iter).GenerateSetStatement(output, work, prefix, brackets);
    }
  }

private:
  class Setting {
  public:
    Setting(const wxString &input)
    {
      unsigned splitpt = input.find(_T('='));
      name = input.Left(splitpt);
      value = input.Mid(splitpt + 1);
    }

    template <class OutputIterator>
    void GenerateSetStatement(OutputIterator output, ObjectBrowserWork *work, const wxString &prefix = wxEmptyString, bool brackets = false) const
    {
      wxString sql = prefix;
      sql << _T("SET ");
      if (brackets)
        sql << _T("( ");
      sql << name << _T(" = ");
      if (name == _T("search_path"))
        sql << value;
      else
        sql << work->QuoteLiteral(value);
      if (brackets)
        sql << _T(" )");
      *output++ = sql;
    }

  private:
    wxString name;
    wxString value;
  };

  std::vector<Setting> settings;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
