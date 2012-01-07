/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __results_book_h
#define __results_book_h

#include "wx/notebook.h"
#include "wx/stc/stc.h"
#include "pqwx.h"
#include "script_events.h"
#include "query_results.h"

/**
 * Notebook widget containing messages and result grids from a script execution.
 */
class ResultsNotebook : public wxNotebook {
public:
  /**
   * Create notebook.
   */
  ResultsNotebook(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) : wxNotebook(parent, id, pos, size) { Setup(); }

  /**
   * Reset notebook.
   * This deletes all the pages if any are present.
   */
  void Reset() { DeleteAllPages(); Setup(); }

  /**
   * Add command completion to messages.
   */
  void ScriptCommandCompleted(const wxString& statusTag);
  /**
   * Add a result set.
   */
  void ScriptResultSet(const wxString &statusTag,
		       const QueryResults &data);
  /**
   * Add an error.
   */
  void ScriptError(const PgError &error);
  /**
   * Add a notice message.
   */
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

// Local Variables:
// mode: c++
// End:
