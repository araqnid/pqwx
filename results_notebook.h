/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __results_book_h
#define __results_book_h

#include "wx/notebook.h"
#include "pqwx.h"
#include "script_events.h"
#include "query_results.h"

class wxHtmlWindow;
class wxHtmlCellEvent;

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
  void ScriptCommandCompleted(const wxString& statusTag, const wxString &query, unsigned scriptPosition);
  /**
   * Add a result set.
   */
  void ScriptResultSet(const wxString &statusTag, const QueryResults &data, const wxString &query, unsigned scriptPosition);
  /**
   * Add a server error.
   */
  void ScriptError(const PgError &error, const wxString &query, unsigned scriptPosition);
  /**
   * Add an internal error.
   */
  void ScriptInternalError(const wxString &error, const wxString &query, unsigned scriptPosition);
  /**
   * Add a message output by a script using the echo command.
   */
  void ScriptEcho(const wxString &message, unsigned scriptPosition);
  /**
   * Add a notice message that occurred while executing a query.
   */
  void ScriptQueryNotice(const PgError &notice, const wxString &query, unsigned scriptPosition);
  /**
   * Add a notice message that occurred asynchronously.
   */
  void ScriptAsynchronousNotice(const PgError &notice);

private:
  wxPanel *messagesPanel;
  wxHtmlWindow *messagesDisplay;

  void AddResultSet(wxPanel *parent, const QueryResults &data);
  bool addedResultSet;
  bool addedError;

  void Setup();

  void AppendServerMessage(const PgError &message, const wxString &color = _T("000000"), bool bold = false);

  void OnMessageClicked(wxHtmlCellEvent&);

  DECLARE_EVENT_TABLE()
};

#endif

// Local Variables:
// mode: c++
// End:
