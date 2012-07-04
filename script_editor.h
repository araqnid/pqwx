/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __script_editor_h
#define __script_editor_h

#include "wx/stc/stc.h"
#include "connect_dialogue.h"

class ScriptEditorPane;

/**
 * Script editor widget.
 */
class ScriptEditor : public wxStyledTextCtrl {
public:
  /**
   * Create script editor.
   */
  ScriptEditor(wxWindow *parent, wxWindowID id, ScriptEditorPane *owner);

  void OnSetFocus(wxFocusEvent &event);
  void OnLoseFocus(wxFocusEvent &event);
  void OnSavePointLeft(wxStyledTextEvent &event);
  void OnSavePointReached(wxStyledTextEvent &event);
  void OnChar(wxKeyEvent& evt);

  /**
   * Load a file into the buffer.
   */
  void LoadFile(const wxString &filename);
  /**
   * Write the buffer to a file.
   */
  void WriteFile(const wxString &filename);

  /**
   * Get the currently-selected region, or the entire buffer.
   * @param lengthp Populated with the length of the region returned
   */
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

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
