#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/splitter.h"
#include "script_editor.h"
#include "script_editor_pane.h"

ScriptEditorPane::ScriptEditorPane(wxWindow *parent, wxWindowID id) : wxPanel(parent, id), resultsBook(NULL)
{
  splitter = new wxSplitterWindow(this, wxID_ANY);
  editor = new ScriptEditor(splitter, wxID_ANY);

  wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(splitter, 1, wxEXPAND);
  SetSizer(sizer);

  splitter->Initialize(editor);
  splitter->SetMinimumPaneSize(100);
  splitter->SetSashGravity(1.0);
}
