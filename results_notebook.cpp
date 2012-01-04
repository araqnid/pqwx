#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/grid.h"
#include "pqwx.h"
#include "results_notebook.h"
#include "pg_error.h"

BEGIN_EVENT_TABLE(ResultsNotebook, wxNotebook)
END_EVENT_TABLE()

void ResultsNotebook::Reset()
{
  DeleteAllPages();
  addedResultSet = false;
  addedError = false;

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
}

void ResultsNotebook::ScriptCommandCompleted(const wxString& statusTag)
{
  messagesDisplay->SetReadOnly(false);
  messagesDisplay->AddText(statusTag);
  messagesDisplay->AddText(_T("\n"));
  messagesDisplay->SetReadOnly(true);
}

void ResultsNotebook::ScriptResultSet(const wxString& statusTag,
				      const QueryResults& data)
{
  wxPanel *resultsPanel = new wxPanel(this, wxID_ANY);
  AddPage(resultsPanel, _("&Results"), !addedResultSet && !addedError);
  wxSizer *resultsSizer = new wxBoxSizer(wxVERTICAL);
  resultsPanel->SetSizer(resultsSizer);
  AddResultSet(resultsPanel, data);

  addedResultSet = true;

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
  addedError = true;
  SetSelection(0);
}

void ResultsNotebook::ScriptNotice(const PgError& notice)
{
  messagesDisplay->SetReadOnly(false);
  messagesDisplay->AddText(notice.primary);
  messagesDisplay->AddText(_T("\n"));
  messagesDisplay->SetReadOnly(true);
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
