#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "querytool_frame.h"

class QueryToolApp: public wxApp {
public:
  virtual bool OnInit();
};

IMPLEMENT_APP(QueryToolApp)

bool QueryToolApp::OnInit()
{
  if (!wxApp::OnInit())
    return false;

  QueryToolFrame *frame = new QueryToolFrame(_T("Query tool"));
  frame->Show(true);

  return true;
}

