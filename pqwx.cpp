#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/cmdline.h"
#include "wx/xrc/xmlres.h"
#include "wx/config.h"
#include "pqwx.h"
#include "pqwx_frame.h"
#include "connect_dialogue.h"
#include "jansson.h"

extern void InitXmlResource(void);

IMPLEMENT_APP(PQWXApp)

bool PQWXApp::OnInit()
{
  if (!wxApp::OnInit())
    return false;

  InitXmlResource();
  wxXmlResource::Get()->InitAllHandlers();

  PqwxFrame *frame = new PqwxFrame(_T("PQWX"));
  frame->Show(true);

  ConnectDialogue *connect = new ConnectDialogue(NULL, frame->objectBrowser);
  connect->Show();
  if (haveInitial) {
    connect->DoInitialConnection(initialServer, initialUser, initialPassword);
  }

  return true;
}

void PQWXApp::OnInitCmdLine(wxCmdLineParser &parser) {
  parser.AddOption(_T("S"), _T("server"), _("server host (or host:port)"));
  parser.AddOption(_T("U"), _T("user"), _("login username"));
  parser.AddOption(_T("P"), _T("password"), _("login password"));
  wxApp::OnInitCmdLine(parser);
}

bool PQWXApp::OnCmdLineParsed(wxCmdLineParser &parser) {
  haveInitial = false;

  if (parser.Found(_T("S"), &initialServer)) {
    haveInitial = true;
  }
  if (parser.Found(_T("U"), &initialUser)) {
    haveInitial = true;
  }
  if (parser.Found(_T("P"), &initialPassword)) {
    haveInitial = true;
  }

  return true;
}

list<wxString> PQWXApp::LoadConfigList(const wxString &label) {
  wxConfigBase *cfg = wxConfig::Get();
  list<wxString> result;
  wxString value;

  // empty strings are treated as []
  if (cfg->Read(label, &value) && !value.IsEmpty()) {
    wxLogDebug(_T("Loading configured list: %s %s"), label.c_str(), value.c_str());
    json_error_t jsonError;
    json_t *parsed = json_loads(value.utf8_str(), 0, &jsonError);
    wxASSERT(parsed != NULL);
    wxASSERT(json_is_array(parsed));
    int size = json_array_size(parsed);
    for (int i = 0; i < size; i++) {
      json_t *elem = json_array_get(parsed, i);
      wxASSERT(elem != NULL);
      result.push_back(wxString(json_string_value(elem), wxConvUTF8));
    }
    json_decref(parsed);
  }
  else {
    wxLogDebug(_T("Empty configured list: %s"), label.c_str());
  }

  return result;
}

void PQWXApp::SaveConfigList(const wxString &label, const list<wxString> &values) {
  wxConfigBase *cfg = wxConfig::Get();

  json_t *list = json_array();
  for (std::list<wxString>::const_iterator iter = values.begin(); iter != values.end(); iter++) {
    wxASSERT(json_array_append_new(list, json_string(iter->utf8_str())) == 0);
  }
  wxString encoded = wxString(json_dumps(list, JSON_COMPACT), wxConvUTF8);
  json_decref(list);

  wxLogDebug(_T("Saving configured list: %s %s"), label.c_str(), encoded.c_str());

  cfg->Write(label, encoded);
}
