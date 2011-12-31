// -*- mode: c++ -*-

#ifndef __results_book_h
#define __results_book_h

#include "wx/notebook.h"
#include "pqwx.h"

class ResultsNotebook : public wxNotebook {
public:
  ResultsNotebook(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) : wxNotebook(parent, id, pos, size) {
    resultsPanel = new wxPanel(this, Pqwx_ResultsPage);
    AddPage(resultsPanel, _("&Results"), true);

    messagesPanel = new wxPanel(this, Pqwx_MessagesPage);
    AddPage(messagesPanel, _("&Messages"), false);

    planPanel = new wxPanel(this, Pqwx_PlanPage);
    AddPage(planPanel, _("&Query Plan"), false);
  }

private:
  wxPanel *resultsPanel;
  wxPanel *messagesPanel;
  wxPanel *planPanel;
  DECLARE_EVENT_TABLE()
};

#endif
