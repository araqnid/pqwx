// -*- mode: c++ -*-

#ifndef __script_editor_h
#define __script_editor_h

#include "wx/stc/stc.h"

class ScriptModel;
class ScriptsNotebook;

class ScriptEditor : public wxStyledTextCtrl {
public:
  ScriptEditor(ScriptsNotebook *owner, wxWindowID id);

  void OnSetFocus(wxFocusEvent &event);

private:
  ScriptsNotebook *owner;

  DECLARE_EVENT_TABLE()
};

#endif
