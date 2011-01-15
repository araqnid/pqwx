#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "pqwx_frame.h"

class PQWXApp: public wxApp {
public:
  virtual bool OnInit();
};

IMPLEMENT_APP(PQWXApp)

bool PQWXApp::OnInit()
{
  if (!wxApp::OnInit())
    return false;

  PqwxFrame *frame = new PqwxFrame(_T("Query tool"));
  frame->Show(true);

  return true;
}

