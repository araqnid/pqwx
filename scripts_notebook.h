// -*- mode: c++ -*-

#ifndef __scripts_book_h
#define __scripts_book_h

#include "wx/notebook.h"
#include "pqwx.h"

class ScriptsNotebook : public wxNotebook {
public:
  ScriptsNotebook(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) : wxNotebook(parent, id, pos, size) {
  }

private:
  DECLARE_EVENT_TABLE()
};

#endif
