#include "wx/wx.h"
#include "wx/cmdline.h"
#include "libpq-fe.h"
#include "object_browser.h"
#include "versioned_sql.h"

extern VersionedSql *GetObjectBrowserSql();

class DumpCatalogueApp : public wxAppConsole {
public:
  DumpCatalogueApp() : sqlDictionary(&(ObjectBrowser::GetSqlDictionary())) {}
  int OnRun();
  void OnInitCmdLine(wxCmdLineParser &parser);
  bool OnCmdLineParsed(wxCmdLineParser &parser);
private:
  const VersionedSql *sqlDictionary;
  wxString dbname;
};

IMPLEMENT_APP(DumpCatalogueApp)

int DumpCatalogueApp::OnRun() {
  wxString conninfo;
  if (!dbname.IsEmpty()) conninfo << _T("dbname=") << dbname;
  PGconn *conn = PQconnectdb(conninfo.utf8_str());
  ConnStatusType connStatus = PQstatus(conn);
  wxCHECK_MSG(connStatus == CONNECTION_OK, 1, wxString(PQerrorMessage(conn), wxConvUTF8));
  const char *sql = sqlDictionary->GetSql(_T("IndexSchema"), PQserverVersion(conn));
  PGresult *rs = PQexec(conn, sql);
  wxCHECK(rs != NULL, 1);
  ExecStatusType status = PQresultStatus(rs);
  wxCHECK_MSG(status == PGRES_TUPLES_OK, 1, wxString(PQresultErrorMessage(rs), wxConvUTF8));
  for (int rowNum = 0; rowNum < PQntuples(rs); rowNum++) {
    for (int colNum = 0; colNum < PQnfields(rs); colNum++) {
      const char *value = PQgetvalue(rs, rowNum, colNum);
      if (colNum > 0) std::cout << '|';
      if (value != NULL) std::cout << value;
    }
    std::cout << std::endl;
  }
  PQclear(rs);
  PQfinish(conn);
  return 0;
}

void DumpCatalogueApp::OnInitCmdLine(wxCmdLineParser &parser) {
  parser.AddParam(_T("dbname"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
  wxAppConsole::OnInitCmdLine(parser);
}

bool DumpCatalogueApp::OnCmdLineParsed(wxCmdLineParser &parser) {
  if (parser.GetParamCount() > 0)
    dbname = parser.GetParam(0);
  return true;
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
