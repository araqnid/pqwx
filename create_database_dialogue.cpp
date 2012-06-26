#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/xrc/xmlres.h"
#include "wx/notebook.h"
#include "wx/clipbrd.h"
#include "pqwx_util.h"
#include "create_database_dialogue.h"

BEGIN_EVENT_TABLE(CreateDatabaseDialogue, wxDialog)
  EVT_BUTTON(wxID_OK, CreateDatabaseDialogue::OnExecute)
  EVT_BUTTON(wxID_CANCEL, CreateDatabaseDialogue::OnCancel)
  EVT_NOTEBOOK_PAGE_CHANGED(XRCID("tabs"), CreateDatabaseDialogue::OnTabChanged)
  PQWX_OBJECT_BROWSER_WORK_FINISHED(wxID_ANY, CreateDatabaseDialogue::OnWorkFinished)
  PQWX_OBJECT_BROWSER_WORK_CRASHED(wxID_ANY, CreateDatabaseDialogue::OnWorkCrashed)
END_EVENT_TABLE()

CreateDatabaseDialogue::CreateDatabaseDialogue(wxWindow *parent, WorkLauncher *launcher, ExecutionCallback *executionCallback) : wxDialog(), launcher(launcher), executionCallback(executionCallback), tabsNotebook(NULL)
{
  InitXRC(parent);
  launcher->DoWork(new ListUsersWork(), this);
}

CreateDatabaseDialogue::~CreateDatabaseDialogue()
{
  if (executionCallback != NULL) delete executionCallback;
  delete launcher;
}

void CreateDatabaseDialogue::InitXRC(wxWindow *parent)
{
  wxXmlResource::Get()->LoadDialog(this, parent, _T("create_database"));

  modeInput = XRCCTRL(*this, "mode", wxChoice);
  tabsNotebook = XRCCTRL(*this, "tabs", wxNotebook);
  scriptInput = XRCCTRL(*this, "script", wxTextCtrl);

  ownerInput = XRCCTRL(*this, "owner", wxComboBox);
  nameInput = XRCCTRL(*this, "database_name", wxTextCtrl);
  encodingInput = XRCCTRL(*this, "encoding", wxComboBox);
  localeInput = XRCCTRL(*this, "locale", wxComboBox);
  collationInput = XRCCTRL(*this, "collation", wxComboBox);
  collationOverrideInput = XRCCTRL(*this, "collation_override", wxCheckBox);
  ctypeInput = XRCCTRL(*this, "ctype", wxComboBox);
  ctypeOverrideInput = XRCCTRL(*this, "ctype_override", wxCheckBox);
}

void CreateDatabaseDialogue::OnTabChanged(wxNotebookEvent& event)
{
  if (tabsNotebook == NULL) return;
  unsigned page = event.GetSelection();
  if (page == tabsNotebook->GetPageCount() - 1) {
    wxString sql;
    GenerateScript(WxStringConcatenator(sql, _T("\n\n")));
    scriptInput->SetValue(sql);
  }
}

void CreateDatabaseDialogue::OnExecute(wxCommandEvent& event)
{
  switch (modeInput->GetCurrentSelection()) {
  case MODE_EXECUTE: {
    std::vector<wxString> commands;
    GenerateScript(std::back_inserter(commands));
    launcher->DoWork(new ExecuteScriptWork(commands), this);
    return; // don't destroy the dialog
  }
    break;

  case MODE_WINDOW: {
    wxString script;
    GenerateScript(WxStringConcatenator(script, _T("\n\n")));
    PQWXDatabaseEvent evt(launcher->GetServerConnection(), launcher->GetDatabaseName(), PQWX_ScriptToWindow);
    evt.SetString(script);
    // wxDialog will not propagate events up, but we need to reach the frame.
    GetParent()->ProcessEvent(evt);
  }
    break;

  case MODE_CLIPBOARD: {
    wxString script;
    GenerateScript(WxStringConcatenator(script, _T("\n\n")));
    if (wxTheClipboard->Open()) {
      wxTheClipboard->SetData(new wxTextDataObject(script));
      wxTheClipboard->Close();
    }
  }
    break;

  case MODE_FILE:
    wxMessageBox(_T("TODO"));
    break;
  }

  Destroy();
}

void CreateDatabaseDialogue::ExecuteScriptWork::operator()()
{
  for (std::vector<wxString>::const_iterator iter = commands.begin(); iter != commands.end(); iter++) {
    owner->DatabaseWork::DoCommand(*iter);
  }
}

void CreateDatabaseDialogue::OnScriptExecuted(Work *work)
{
  if (executionCallback != NULL)
    executionCallback->ActionPerformed();
  Destroy();
}

void CreateDatabaseDialogue::OnCancel(wxCommandEvent& event)
{
  Destroy();
}

#define CALL_WORK_COMPLETION(dbox, handler) ((dbox).*(handler))

void CreateDatabaseDialogue::OnWorkFinished(wxCommandEvent& event)
{
  Work *work = static_cast<Work*>(event.GetClientData());
  wxASSERT(work != NULL);
  wxLogDebug(_T("%p: work finished (on create database dialogue)"));
  CALL_WORK_COMPLETION(*this, work->completionHandler)(work);
  delete work;
}

void CreateDatabaseDialogue::OnWorkCrashed(wxCommandEvent& event)
{
  Work *work = static_cast<Work*>(event.GetClientData());
  wxLogDebug(_T("%p: work crashed (on create database dialogue)"));

  if (!work->GetCrashMessage().empty()) {
    wxLogError(_T("%s\n%s"), _("An unexpected error occurred interacting with the database. Failure will ensue."), work->GetCrashMessage().c_str());
  }
  else {
    wxLogError(_T("%s"), _("An unexpected and unidentified error occurred interacting with the database. Failure will ensue."));
  }

  delete work;
}

void CreateDatabaseDialogue::ListUsersWork::operator()()
{
  QueryResults rs = Query(_T("List users")).List();
  for (QueryResults::const_iterator iter = rs.begin(); iter != rs.end(); iter++) {
    result.push_back((*iter)[0]);
  }
}

void CreateDatabaseDialogue::OnListUsersComplete(Work *work)
{
  ListUsersWork *listUsersWork = static_cast<ListUsersWork*>(work);
  wxLogDebug(_T("Listing users complete"));
  const std::vector<wxString>& result = listUsersWork->result;
  for (std::vector<wxString>::const_iterator iter = result.begin(); iter != result.end(); iter++) {
    ownerInput->Append(*iter);
  }
}

// This is all rather nasty. We don't have a PGconn* object around if we're generating the script in the UI thread.
// Would have to swap over to the worker thread just to generate the script otherwise... maybe not a totally bad idea.
// For now, have these over-simple implementations here.

static bool IsSimpleSymbol(const char *str) {
  for (const char *p = str; *p != '\0'; p++) {
    if (!( (*p >= 'a' && *p <= 'z') || *p == '_' || (*p >= '0' && *p <= '9') ))
      return false;
  }
  return true;
}

wxString CreateDatabaseDialogue::QuoteLiteral(const wxString &str)
{
  wxString escaped;
  escaped << _T('\'') << str << _T('\'');
  return escaped;
}

wxString CreateDatabaseDialogue::QuoteIdent(const wxString &str)
{
  if (IsSimpleSymbol(str.utf8_str())) return str;
  wxString escaped;
  escaped << _T('\"') << str << _T('\"');
  return escaped;
}

template<class T>
void CreateDatabaseDialogue::GenerateScript(T output)
{
  wxString sql;
  sql << _T("CREATE DATABASE ") << QuoteIdent(nameInput->GetValue());

  wxString encoding = encodingInput->GetValue();
  if (!encoding.empty()) sql << _T("\n\tENCODING = ") << QuoteLiteral(encoding);
  if (collationOverrideInput->IsChecked())
    sql << _T("\n\tLC_COLLATE = ") << QuoteLiteral(collationInput->GetValue());
  if (ctypeOverrideInput->IsChecked())
    sql << _T("\n\tLC_CTYPE = ") << QuoteLiteral(collationInput->GetValue());
  wxString owner = ownerInput->GetValue();
  if (!owner.empty()) sql << _T("\n\tOWNER ") << QuoteIdent(owner);

  *output++ = sql;
}
