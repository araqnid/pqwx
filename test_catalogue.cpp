#include "catalogue_index.h"
#include "wx/wx.h"
#include "wx/cmdline.h"
#include <vector>

using namespace std;

class TestCatalogueApp : public wxAppConsole {
public:
  int OnRun();
  void OnInitCmdLine(wxCmdLineParser &parser);
  bool OnCmdLineParsed(wxCmdLineParser &parser);
private:
  vector<wxString> queries;
};

IMPLEMENT_APP(TestCatalogueApp)

int TestCatalogueApp::OnRun() {
  CatalogueIndex index;
  index.Begin();
  index.AddDocument(16394, CatalogueIndex::FUNCTION_SCALAR, _T("public.my_func"));
  index.AddDocument(16395, CatalogueIndex::VIEW, _T("public.running_speed"));
  index.AddDocument(16399, CatalogueIndex::TABLE, _T("so4423225.pref_money"));
  index.AddDocument(62391, CatalogueIndex::TABLE, _T("so8201351.t"));
  index.AddDocument(62401, CatalogueIndex::VIEW, _T("so8201351.my_soln1"));
  index.AddDocument(72236, CatalogueIndex::EXTENSION, _T("public.cube"));
  index.Commit();
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
  index.DumpDocumentStore();
#endif
  for (vector<wxString>::iterator iter = queries.begin(); iter != queries.end(); iter++) {
    index.Search(*iter);
  }
  return 0;
}

void TestCatalogueApp::OnInitCmdLine(wxCmdLineParser &parser) {
  parser.AddParam(_T("query"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE);
  wxAppConsole::OnInitCmdLine(parser);
}

bool TestCatalogueApp::OnCmdLineParsed(wxCmdLineParser &parser) {
  int params = parser.GetParamCount();
  for (int i = 0; i < params; i++) {
    queries.push_back(parser.GetParam(i));
  }
  return true;
}
