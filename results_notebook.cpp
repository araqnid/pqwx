#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/grid.h"
#include "wx/tokenzr.h"
#include "wx/wupdlock.h"
#include "wx/html/htmlwin.h"
#include "pqwx.h"
#include "results_notebook.h"
#include "pg_error.h"

BEGIN_EVENT_TABLE(ResultsNotebook, wxNotebook)
  EVT_HTML_CELL_CLICKED(Pqwx_MessagesDisplay, ResultsNotebook::OnMessageClicked)
END_EVENT_TABLE()

DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptShowPosition)

void ResultsNotebook::Setup()
{
  addedResultSet = false;
  addedError = false;

  messagesPanel = new wxPanel(this, Pqwx_MessagesPage);
  AddPage(messagesPanel, _("&Messages"), true);

  messagesDisplay = new wxHtmlWindow(messagesPanel, Pqwx_MessagesDisplay);
  messagesDisplay->SetFonts(wxNORMAL_FONT->GetFaceName(), wxNORMAL_FONT->GetFaceName());

  wxSizer *displaySizer = new wxBoxSizer(wxVERTICAL);
  displaySizer->Add(messagesDisplay, 1, wxEXPAND);
  messagesPanel->SetSizer(displaySizer);
}

void ResultsNotebook::ScriptCommandCompleted(const wxString& statusTag, const wxString &query, unsigned scriptPosition)
{
  messagesDisplay->AppendToPage(_T("<font color='#000000'>") + statusTag + _T("</font><br>"));
}

void ResultsNotebook::ScriptResultSet(const wxString& statusTag, const QueryResults& data, const wxString& query, unsigned scriptPosition)
{
  wxWindowUpdateLocker noUpdates(this);

  wxPanel *resultsPanel = new wxPanel(this, wxID_ANY);
  AddPage(resultsPanel, _("&Results"), !addedResultSet && !addedError);
  wxSizer *resultsSizer = new wxBoxSizer(wxVERTICAL);
  resultsPanel->SetSizer(resultsSizer);
  AddResultSet(resultsPanel, data);

  addedResultSet = true;

  ScriptCommandCompleted(statusTag, query, scriptPosition);
}

void ResultsNotebook::ScriptError(const PgError& error, const wxString &query, unsigned scriptPosition)
{
  wxWindowUpdateLocker noUpdates(this);

  unsigned linkTarget = scriptPosition;
  if (error.HasPosition()) linkTarget += error.GetPosition();
  messagesDisplay->AppendToPage(wxString::Format(_T("<a href='#%u'>"), linkTarget));
  AppendServerMessage(error, _T("#ff0000"), true);
  messagesDisplay->AppendToPage(_T("</a>"));
  addedError = true;
  SetSelection(0);
}

void ResultsNotebook::ScriptInternalError(const wxString& error, const wxString &query, unsigned scriptPosition)
{
  messagesDisplay->AppendToPage(_T("<font color='#ff0000'><b>") + error + _T("</b></font><br>"));
  addedError = true;
  SetSelection(0);
}

void ResultsNotebook::ScriptEcho(const wxString& message, unsigned scriptPosition)
{
  messagesDisplay->AppendToPage(_T("<font color='#000088'>") + message + _T("</font><br>"));
}

void ResultsNotebook::ScriptQueryNotice(const PgError& notice, const wxString &query, unsigned scriptPosition)
{
  wxWindowUpdateLocker noUpdates(this);
  AppendServerMessage(notice, _T("#0000ff"));
}

void ResultsNotebook::ScriptAsynchronousNotice(const PgError& notice)
{
  wxWindowUpdateLocker noUpdates(this);
  AppendServerMessage(notice, _T("#0000ff"));
}

void ResultsNotebook::ScriptAsynchronousNotification(const wxString& notification)
{
  messagesDisplay->AppendToPage(_T("<font color='#0000ff'><b>") + notification + _T("</b></font><br>"));
  SetSelection(0);
}

void ResultsNotebook::AppendServerMessage(const PgError& message, const wxString &colour, bool bold)
{
  messagesDisplay->AppendToPage(_T("<font color='") + colour + _T("'>"));
  if (bold) messagesDisplay->AppendToPage(_T("<b>"));
  messagesDisplay->AppendToPage(wxString::Format(_T("%s: %s<br>"), message.GetSeverity().c_str(), message.GetPrimary().c_str()));
  if (message.HasDetail()) {
    messagesDisplay->AppendToPage(wxString::Format(_T("%s: %s<br>"), _("DETAIL"), message.GetDetail().c_str()));
  }
  if (message.HasHint()) {
    messagesDisplay->AppendToPage(wxString::Format(_T("%s: %s<br>"), _("HINT"), message.GetHint().c_str()));
  }
  if (message.HasContext()) {
    messagesDisplay->AppendToPage(wxString::Format(_T("%s:<br>"), _("CONTEXT")));
    messagesDisplay->AppendToPage(_T("<ul>"));
    for (std::vector<wxString>::const_iterator iter = message.GetContext().begin(); iter != message.GetContext().end(); iter++) {
      messagesDisplay->AppendToPage(wxString::Format(_T("<li>%s</li>"), (*iter).c_str()));
    }
    messagesDisplay->AppendToPage(_T("</ul>"));
  }
  if (bold) messagesDisplay->AppendToPage(_T("</b>"));
  messagesDisplay->AppendToPage(_T("</font>"));
}

void ResultsNotebook::OnMessageClicked(wxHtmlCellEvent &event)
{
  wxHtmlLinkInfo *info = event.GetCell()->GetLink();

  if (info == NULL) return;

  long position;
  wxCHECK2_MSG(info->GetHref().Mid(1).ToLong(&position), , info->GetHref());

  event.SetLinkClicked(true);

  wxCommandEvent cmd(PQWX_ScriptShowPosition);
  cmd.SetInt((int) position);
  ProcessEvent(cmd);
}

void ResultsNotebook::AddResultSet(wxPanel *parent, const QueryResults &data)
{
  wxGrid *grid = new wxGrid(parent, wxID_ANY);

  const unsigned maxRowsForAutoSize = 500;
  unsigned firstChunkSize = data.size() > maxRowsForAutoSize ? maxRowsForAutoSize : data.size();
  grid->CreateGrid(firstChunkSize, data.Fields().size());
  grid->BeginBatch();

  unsigned columnIndex = 0;
  for (std::vector<QueryResults::Field>::const_iterator iter = data.Fields().begin(); iter != data.Fields().end(); iter++, columnIndex++) {
    grid->SetColLabelValue(columnIndex, (*iter).GetName());
    switch ((*iter).GetType()) {
#if 0
    case 16: // boolean
      grid->SetColFormatBool(columnIndex);
      break;
#endif
    }
  }

  unsigned rowIndex = 0;
  const int fieldCount = (int) data.Fields().size();
  QueryResults::const_iterator rowIter = data.begin();
  for (; rowIter != data.end(); rowIter++, rowIndex++) {
    if (rowIndex >= firstChunkSize) break;
    for (int columnIndex = 0; columnIndex < fieldCount; columnIndex++) {
      const wxString &value = (*rowIter)[columnIndex];
      switch (data.Fields()[columnIndex].GetType()) {
#if 0
      case 16: // boolean
        grid->GetTable()->SetValueAsBool(rowIndex, columnIndex, value == _T("t"));
        break;
#endif

      default:
        grid->SetCellValue(rowIndex, columnIndex, value);
        break;
      }
    }
  }

  wxLogDebug(_T("Added %d rows, calling AutoSize()"), rowIndex);

  grid->AutoSize();

  if (rowIndex < data.size()) {
    wxLogDebug(_T("Finished AutoSize(), adding %d more rows"), data.size() - rowIndex);
    grid->AppendRows(data.size() - rowIndex);

    for (; rowIter != data.end(); rowIter++, rowIndex++) {
      for (int columnIndex = 0; columnIndex < fieldCount; columnIndex++) {
        const wxString &value = (*rowIter)[columnIndex];
        switch (data.Fields()[columnIndex].GetType()) {
#if 0
        case 16: // boolean
          grid->GetTable()->SetValueAsBool(rowIndex, columnIndex, value == _T("t"));
          break;
#endif

        default:
          grid->SetCellValue(rowIndex, columnIndex, value);
          break;
        }
      }
    }
  }

  grid->EndBatch();
  parent->GetSizer()->Add(grid, 1, wxEXPAND);
}
// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
