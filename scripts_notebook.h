// -*- mode: c++ -*-

#ifndef __scripts_book_h
#define __scripts_book_h

#include <vector>
#include "wx/notebook.h"
#include "pqwx.h"

class ScriptModel {
public:
  ScriptModel(const wxString &title, const wxString &filename = wxEmptyString) : title(title), filename(filename) {}
private:
  const wxString title;
  const wxString filename;
};

class ScriptsNotebook : public wxNotebook {
public:
  ScriptsNotebook(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) : wxNotebook(parent, id, pos, size), documentCounter(0) {
  }

  void OpenNewScript();
  void OpenScriptFile(const wxString &filename);

private:
  int documentCounter;
  std::vector<ScriptModel> scripts;

  DECLARE_EVENT_TABLE()
};

#endif
