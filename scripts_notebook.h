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
  ScriptModel(const wxString &title, const wxString &filename = wxEmptyString) : title(title), filename(filename), modified(false) {}
  wxString FormatTitle() {
    wxString output;
    if (database.empty())
      output << _("<disconnected>");
    else
      output = database;
    output << _T(" - ") << title;
    if (modified) output << _T(" *");
    return output;
  }
private:
  ScriptsNotebook *owner;
  wxString title;
  wxString filename;
  wxString database;
  bool modified;
  friend class ScriptsNotebook;
  friend class ScriptEditor;
};

class ScriptsNotebook : public wxNotebook {
public:
  ScriptsNotebook(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) : wxNotebook(parent, id, pos, size), documentCounter(0) { }

  ScriptEditor *OpenNewScript();
  ScriptEditor *OpenScriptFile(const wxString &filename);
  ScriptEditor *OpenScriptWithText(const wxString &text);

  void OnScriptSelected(wxCommandEvent&);

private:
  int documentCounter;
  std::vector<ScriptModel> scripts;

  ScriptModel& FindScriptForEditor(const ScriptEditor *);
  unsigned FindScriptPage(const ScriptModel&) const;

  wxString GenerateDocumentName() { return wxString::Format(_("Query-%d.sql"), ++documentCounter); }

  friend class ScriptEditor;

  DECLARE_EVENT_TABLE()
};

#endif
