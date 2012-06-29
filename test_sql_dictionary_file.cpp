#include "sql_dictionary_file.h"
#include "wx/wx.h"
#include "wx/cmdline.h"
#include <vector>
#include <fstream>

class TestSqlDictionaryFileApp : public wxAppConsole {
public:
  int OnRun();
private:
  void ShowSql(const SqlDictionary& dict, const wxString& key, int serverVersion);
};

IMPLEMENT_APP(TestSqlDictionaryFileApp)

int TestSqlDictionaryFileApp::OnRun()
{
  ShowSql(GetObjectBrowserSqlFile(), _T("Tablespaces"), 80412);
  ShowSql(GetObjectBrowserSqlFile(), _T("Tablespaces"), 90200);

  ShowSql(GetObjectBrowserSqlFile(), _T("Relations"), 90200);

  ShowSql(GetObjectBrowserScriptsSqlFile(), _T("Database Detail"), 80412);

  ShowSql(GetDependenciesViewFile(), _T("Dependents"), 80412);

  return 0;
}

void TestSqlDictionaryFileApp::ShowSql(const SqlDictionary& dict, const wxString& key, int serverVersion)
{
  const char *rawSql = dict.GetSql(key, serverVersion);
  wxString sql(rawSql, wxConvUTF8);
  wxLogDebug(_T("%s:%d:\n%s"), key.c_str(), serverVersion, sql.c_str());
}
