#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/wupdlock.h"
#include "pqwx.h"
#include "results_notebook.h"
#include "pg_error.h"

BEGIN_EVENT_TABLE(ResultsNotebook, wxNotebook)
END_EVENT_TABLE()

void ResultsNotebook::Reset()
{
  wxWindowUpdateLocker noUpdates(this);

  DeleteAllPages();

  resultsPanel = NULL;
  planPanel = NULL;

  //resultsPanel = new wxPanel(this, Pqwx_ResultsPage);
  //AddPage(resultsPanel, _("&Results"), true);

  messagesPanel = new wxPanel(this, Pqwx_MessagesPage);
  AddPage(messagesPanel, _("&Messages"), true);

  messagesDisplay = new wxStyledTextCtrl(messagesPanel, Pqwx_MessagesDisplay);
  messagesDisplay->SetReadOnly(true);
#if wxUSE_UNICODE
  messagesDisplay->SetCodePage(wxSTC_CP_UTF8);
#endif
  messagesDisplay->StyleClearAll();

  wxSizer *displaySizer = new wxBoxSizer(wxVERTICAL);
  displaySizer->Add(messagesDisplay, 1, wxEXPAND);
  messagesPanel->SetSizer(displaySizer);

  //planPanel = new wxPanel(this, Pqwx_PlanPage);
  //AddPage(planPanel, _("&Query Plan"), false);
}

void ResultsNotebook::ScriptCommandCompleted(const wxString& statusTag)
{
  messagesDisplay->SetReadOnly(false);
  messagesDisplay->AddText(statusTag);
  messagesDisplay->AddText(_T("\n"));
  messagesDisplay->SetReadOnly(true);
}

void ResultsNotebook::ScriptResultSet(const wxString& statusTag, const QueryResults& data)
{
  messagesDisplay->SetReadOnly(false);
  messagesDisplay->AddText(statusTag);
  messagesDisplay->AddText(_T("\n"));
  messagesDisplay->SetReadOnly(true);
}

void ResultsNotebook::ScriptError(const PgError& error)
{
  messagesDisplay->SetReadOnly(false);
  messagesDisplay->AddText(error.primary);
  messagesDisplay->AddText(_T("\n"));
  messagesDisplay->SetReadOnly(true);
}

void ResultsNotebook::ScriptNotice(const PgError& notice)
{
  messagesDisplay->SetReadOnly(false);
  messagesDisplay->AddText(notice.primary);
  messagesDisplay->AddText(_T("\n"));
  messagesDisplay->SetReadOnly(true);
}
