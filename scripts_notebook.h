// -*- mode: c++ -*-

#ifndef __scripts_book_h
#define __scripts_book_h

#include <vector>
#include "wx/notebook.h"
#include "pqwx.h"

class ScriptsNotebook;
class ScriptEditor;

class ScriptModel {
public:
  ScriptModel(const wxString &title, const wxString &filename = wxEmptyString) : title(title), filename(filename) {}
private:
  wxString title;
  wxString filename;
  friend class ScriptEditor;
  friend class ScriptsNotebook;
};

class ScriptsNotebook : public wxNotebook {
public:
  ScriptsNotebook(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) : wxNotebook(parent, id, pos, size), documentCounter(0) {
  }

  void OpenNewScript();
  void OpenScriptFile(const wxString &filename);
  void OpenScriptWithText(const wxString &text);

private:
  int documentCounter;
  std::vector<ScriptModel> scripts;

  ScriptModel& FindScriptForEditor(const ScriptEditor *);

  friend class ScriptEditor;

  DECLARE_EVENT_TABLE()
};

#endif
