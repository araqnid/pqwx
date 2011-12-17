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
  vector<CatalogueIndex::Type> ParseTypeListString(const wxString &input);
};

IMPLEMENT_APP(TestCatalogueApp)

int TestCatalogueApp::OnRun() {
  CatalogueIndex index;
  index.Begin();
  index.AddDocument(CatalogueIndex::Document(1259, CatalogueIndex::TABLE, true, _T("pg_catalog.pg_class")));
  index.AddDocument(CatalogueIndex::Document(16394, CatalogueIndex::FUNCTION_SCALAR, false, _T("public.my_func")));
  index.AddDocument(CatalogueIndex::Document(16395, CatalogueIndex::VIEW, false, _T("public.running_speed")));
  index.AddDocument(CatalogueIndex::Document(16399, CatalogueIndex::TABLE, false, _T("so4423225.pref_money")));
  index.AddDocument(CatalogueIndex::Document(62391, CatalogueIndex::TABLE, false, _T("so8201351.t")));
  index.AddDocument(CatalogueIndex::Document(62401, CatalogueIndex::VIEW, false, _T("so8201351.my_soln1")));
  index.AddDocument(CatalogueIndex::Document(72236, CatalogueIndex::EXTENSION, false, _T("public.cube")));
  index.Commit();
#ifdef PQWX_DEBUG_CATALOGUE_INDEX
  index.DumpDocumentStore();
#endif
  bool includeSystem = false;
  CatalogueIndex::Filter universal(index.CreateMatchEverythingFilter());
  CatalogueIndex::Filter nonSystem(index.CreateNonSystemFilter());
  CatalogueIndex::Filter typesFilter(index.CreateMatchEverythingFilter());
  for (vector<wxString>::iterator iter = queries.begin(); iter != queries.end(); iter++) {
    if (iter->IsSameAs(_T("+S"))) {
      includeSystem = true;
    }
    else if (iter->IsSameAs(_T("-S"))) {
      includeSystem = false;
    }
    else if ((*iter)[0] == _T('@')) {
      typesFilter.Clear();
      vector<CatalogueIndex::Type> types = ParseTypeListString((*iter).Mid(1));
      for (vector<CatalogueIndex::Type>::iterator iter = types.begin(); iter != types.end(); iter++) {
	typesFilter |= index.CreateTypeFilter(*iter);
      }
    }
    else {
      index.Search(*iter, (includeSystem ? universal : nonSystem) & typesFilter);
    }
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

vector<CatalogueIndex::Type> TestCatalogueApp::ParseTypeListString(const wxString &input) {
  vector<CatalogueIndex::Type> result;
  map<wxString, CatalogueIndex::Type> typeMap;
  typeMap[_T("t")] = CatalogueIndex::TABLE;
  typeMap[_T("v")] = CatalogueIndex::VIEW;
  typeMap[_T("f")] = CatalogueIndex::FUNCTION_SCALAR; // actually any function...
  typeMap[_T("fs")] = CatalogueIndex::FUNCTION_ROWSET;
  typeMap[_T("ft")] = CatalogueIndex::FUNCTION_TRIGGER;
  typeMap[_T("fa")] = CatalogueIndex::FUNCTION_AGGREGATE;
  typeMap[_T("fw")] = CatalogueIndex::FUNCTION_WINDOW;
  typeMap[_T("T")] = CatalogueIndex::TYPE;
  typeMap[_T("O")] = CatalogueIndex::COLLATION;
  typeMap[_T("x")] = CatalogueIndex::EXTENSION;
  wxWCharBuffer buffer = input.wc_str();
  wchar_t *ptr;
  wchar_t *tok;

  for (tok = wcstok(buffer.data(), L",", &ptr); tok != NULL; tok = wcstok(NULL, L",", &ptr)) {
    wxASSERT(typeMap.count(tok) > 0);
    result.push_back(typeMap[tok]);
  }

  return result;
}
