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
  void WriteFile(const wxString &filename);

  wxCharBuffer GetRegion(int *lengthp);
private:
  void UpdateStateInUI();

  ScriptEditorPane *owner;

  static wxString WordList_keywords;
  static wxString WordList_database_objects;
  static wxString WordList_sqlplus;
  static wxString WordList_user1;
  static wxString WordList_user2;
  static wxString WordList_user3;
  static wxString WordList_user4;

  DECLARE_EVENT_TABLE()
};

#endif
