// -*- mode: c++ -*-

#ifndef __script_editor_pane_h
#define __script_editor_pane_h

class ScriptEditor;
class ResultsNotebook;
class wxSplitterWindow;

class ScriptEditorPane : public wxPanel {
public:
  ScriptEditorPane(wxWindow *parent, wxWindowID id = wxID_ANY);

  ScriptEditor *GetEditor() const { return editor; }
  ResultsNotebook *GetResults() const { return resultsBook; }
private:
  ScriptEditor *editor;
  wxSplitterWindow *splitter;
  ResultsNotebook *resultsBook;
};

#endif
