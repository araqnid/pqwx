// -*- mode: c++ -*-

#ifndef __results_book_h
#define __results_book_h

#include "wx/notebook.h"
#include "wx/stc/stc.h"
#include "pqwx.h"
#include "script_events.h"
#include "query_results.h"

class ResultsNotebook : public wxNotebook {
public:
  ResultsNotebook(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) : wxNotebook(parent, id, pos, size) { Setup(); }

  void Reset() { DeleteAllPages(); Setup(); }

  void ScriptCommandCompleted(const wxString& statusTag);
  void ScriptResultSet(const wxString &statusTag,
		       const QueryResults &data);
  void ScriptError(const PgError &error);
  void ScriptNotice(const PgError &notice);

private:
  wxPanel *messagesPanel;
  wxStyledTextCtrl *messagesDisplay;

  void AddResultSet(wxPanel *parent, const QueryResults &data);
  bool addedResultSet;
  bool addedError;

  void Setup();

  DECLARE_EVENT_TABLE()
};

#endif
