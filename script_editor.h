// -*- mode: c++ -*-

#ifndef __script_editor_h
#define __script_editor_h

#include "wx/stc/stc.h"
#include "database_connection.h"

class ScriptModel;
class ScriptsNotebook;

class ScriptEditor : public wxStyledTextCtrl {
public:
  ScriptEditor(ScriptsNotebook *owner, wxWindowID id);
  ~ScriptEditor() {
    if (db != NULL) {
      db->Dispose();
      delete db;
    }
  }

  void OnSetFocus(wxFocusEvent &event);
  void OnSavePointLeft(wxStyledTextEvent &event);
  void OnSavePointReached(wxStyledTextEvent &event);

  void Connect(const ServerConnection &server, const wxString &dbname);
  bool HasConnection() const { return db != NULL; }
  bool IsConnected() const { return db != NULL && db->IsConnected(); }
  wxString ConnectionIdentification() const { wxASSERT(db != NULL); return db->Identification(); }

private:
  ScriptsNotebook *owner;
  DatabaseConnection *db;

  DECLARE_EVENT_TABLE()
};

#endif
