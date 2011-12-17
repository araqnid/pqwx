#include "catalogue_index.h"
#include "wx/wx.h"
#include "wx/cmdline.h"
#include <vector>
#include <fstream>

using namespace std;

class TestCatalogueApp : public wxAppConsole {
public:
  int OnRun();
  void OnInitCmdLine(wxCmdLineParser &parser);
  bool OnCmdLineParsed(wxCmdLineParser &parser);
private:
  vector<wxString> queries;
  vector<CatalogueIndex::Type> ParseTypeListString(const wxString &input);
  wxString inputFile;
};

IMPLEMENT_APP(TestCatalogueApp)

static vector<wxString> Split(const wxString& input, const wxChar sep) {
  vector<wxString> result;

  int mark = 0;
  for (int i = 0; i < input.length(); i++) {
    if (input[i] == sep) {
      result.push_back(input.Mid(mark, i-mark));
      mark = i + 1;
    }
  }
  result.push_back(input.Mid(mark));

  return result;
}

static map<wxString, CatalogueIndex::Type> getTypeMap() {
  map<wxString, CatalogueIndex::Type> typeMap;
  typeMap[_T("t")] = CatalogueIndex::TABLE;
  typeMap[_T("v")] = CatalogueIndex::VIEW;
  typeMap[_T("f")] = CatalogueIndex::FUNCTION_SCALAR;
  typeMap[_T("fs")] = CatalogueIndex::FUNCTION_ROWSET;
  typeMap[_T("ft")] = CatalogueIndex::FUNCTION_TRIGGER;
  typeMap[_T("fa")] = CatalogueIndex::FUNCTION_AGGREGATE;
  typeMap[_T("fw")] = CatalogueIndex::FUNCTION_WINDOW;
  typeMap[_T("T")] = CatalogueIndex::TYPE;
  typeMap[_T("O")] = CatalogueIndex::COLLATION;
  typeMap[_T("x")] = CatalogueIndex::EXTENSION;
  return typeMap;
}

int TestCatalogueApp::OnRun() {
  CatalogueIndex index;
  map<wxString, CatalogueIndex::Type> typeMap = getTypeMap();
  index.Begin();
  ifstream inputStream(inputFile.fn_str());
  char buf[8192];
  do {
    inputStream.getline(buf, sizeof(buf));
    if (inputStream.eof())
      break;
    vector<wxString> parts = Split(wxString(buf, wxConvUTF8), _T('|'));
    wxASSERT(parts.size() == 4);
    long entityId;
    parts[0].ToLong(&entityId);
    CatalogueIndex::Type entityType;
    bool systemObject;
    if (parts[1][parts[1].length()-1] == _T('S')) {
      systemObject = true;
      wxString typeString(parts[1].Left(parts[1].length()-1));
      wxASSERT(typeMap.count(typeString) > 0);
      entityType = typeMap[typeString];
    }
    else {
      systemObject = false;
      wxASSERT(typeMap.count(parts[1]) > 0);
      entityType = typeMap[parts[1]];
    }
    index.AddDocument(CatalogueIndex::Document(entityId, entityType, systemObject, parts[2], parts[3]));
  } while (1);
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
	wxLogDebug(_T("Filtering on %s"), CatalogueIndex::EntityTypeName(*iter).c_str());
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
  parser.AddParam(_T("catalogue-file"), wxCMD_LINE_VAL_STRING);
  parser.AddParam(_T("query"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE);
  wxAppConsole::OnInitCmdLine(parser);
}

bool TestCatalogueApp::OnCmdLineParsed(wxCmdLineParser &parser) {
  inputFile = parser.GetParam(0);
  int params = parser.GetParamCount();
  for (int i = 1; i < params; i++) {
    queries.push_back(parser.GetParam(i));
  }
  return true;
}

vector<CatalogueIndex::Type> TestCatalogueApp::ParseTypeListString(const wxString &input) {
  vector<CatalogueIndex::Type> result;
  wxWCharBuffer buffer = input.wc_str();
  wchar_t *ptr;
  wchar_t *tok;
  map<wxString, CatalogueIndex::Type> typeMap = getTypeMap();

  for (tok = wcstok(buffer.data(), L",", &ptr); tok != NULL; tok = wcstok(NULL, L",", &ptr)) {
    wxASSERT(typeMap.count(tok) > 0);
    result.push_back(typeMap[tok]);
  }

  return result;
}
