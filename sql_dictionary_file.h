/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __sql_dictionary_file_h
#define __sql_dictionary_file_h

#include <list>
#include "sql_dictionary.h"
#include "wx/stream.h"

class SqlDictionaryFile : public SqlDictionary {
public:
  SqlDictionaryFile(const wxString& filename);

private:
  static wxString CollapseSql(const wxString& sql);
  static wxString EscapeC(const wxString& str);
  static wxString ReadLine(wxInputStream* stream);
  std::list<wxCharBuffer> buffers;
  class Tag {
  public:
    Tag() : line(0), majorVersion(0), minorVersion(0) {}
    Tag(const wxString& name, int line) : name(name), line(line), majorVersion(0), minorVersion(0) {}
    Tag(const wxString& name, int line, int majorVersion, int minorVersion) : name(name), line(line), majorVersion(majorVersion), minorVersion(minorVersion) {}
    wxString name;
    int line;
    int majorVersion;
    int minorVersion;
  };
  class StringSplitter {
  public:
    StringSplitter(const wxString& str, const wxString& delimiter) : str(str), delimiter(delimiter), ptr(0) {}
    bool HasMoreTokens() const
    {
      return ptr < str.length();
    }
    wxString GetNextToken()
    {
      wxString search(str);
      if (ptr > 0) search = search.Mid(ptr);
      int len = search.Find(delimiter);
      if (len == wxNOT_FOUND) {
	ptr = str.length();
	return search;
      }
      ptr += len + delimiter.length();
      return search.Left(len);
    }
  private:
    const wxString str;
    const wxString delimiter;
    size_t ptr;
  };
};

#endif
