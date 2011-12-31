// -*- mode: c++ -*-

#ifndef __script_editor_h
#define __script_editor_h

#include "wx/stc/stc.h"

class ScriptModel;

class ScriptEditor : public wxStyledTextCtrl {
public:
  ScriptEditor(wxWindow *parent, wxWindowID id, ScriptModel *model) : wxStyledTextCtrl(parent, id), model(model) {}

private:
  ScriptModel *model;

  DECLARE_EVENT_TABLE()
};

#endif
