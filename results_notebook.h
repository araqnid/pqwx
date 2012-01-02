// -*- mode: c++ -*-

#ifndef __results_book_h
#define __results_book_h

#include "wx/notebook.h"
#include "wx/stc/stc.h"
#include "pqwx.h"
#include "script_events.h"
#include "query_results.h"

class ResultsNotebook : public wxNotebook, public ExecutionResultsHandler {
public:
  ResultsNotebook(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) : wxNotebook(parent, id, pos, size), resultsPanel(NULL), messagesPanel(NULL), messagesDisplay(NULL), planPanel(NULL) { }

  void Reset();

  void ScriptCommandCompleted(const wxString& statusTag);
  void ScriptResultSet(const wxString &statusTag, const QueryResults &data);
  void ScriptError(const PgError &error);
  void ScriptNotice(const PgError &notice);

private:
  wxPanel *resultsPanel;
  wxPanel *messagesPanel;
  wxStyledTextCtrl *messagesDisplay;
  wxPanel *planPanel;
  DECLARE_EVENT_TABLE()
};

#endif
