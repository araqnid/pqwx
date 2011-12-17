#ifndef __pqwx_h
#define __pqwx_h

#include <list>

class PQWXApp: public wxApp {
public:
  virtual bool OnInit();
  void OnInitCmdLine(wxCmdLineParser &parser);
  bool OnCmdLineParsed(wxCmdLineParser &parser);
private:
  bool haveInitial;
  wxString initialServer;
  wxString initialUser;
  wxString initialPassword;

public:
  static std::list<wxString> LoadConfigList(const wxString &label);
  static void SaveConfigList(const wxString &label, const std::list<wxString> &values);
};

#endif
