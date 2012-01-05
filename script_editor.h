// -*- mode: c++ -*-

#ifndef __script_editor_h
#define __script_editor_h

#include "wx/stc/stc.h"
#include "connect_dialogue.h"

class ScriptEditorPane;

class ScriptEditor : public wxStyledTextCtrl {
public:
  ScriptEditor(wxWindow *parent, wxWindowID id, ScriptEditorPane *owner);

  void OnSetFocus(wxFocusEvent &event);
  void OnLoseFocus(wxFocusEvent &event);
  void OnSavePointLeft(wxStyledTextEvent &event);
  void OnSavePointReached(wxStyledTextEvent &event);

  void LoadFile(const wxString &filename);

  wxCharBuffer GetRegion(int *lengthp);
private:
  void UpdateStateInUI();

  ScriptEditorPane *owner;

  DECLARE_EVENT_TABLE()
};

#endif
