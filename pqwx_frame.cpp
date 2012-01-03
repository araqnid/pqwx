#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/aboutdlg.h"
#include "wx/listbox.h"
#include "wx/notebook.h"
#include "wx/config.h"
#include "wx/regex.h"
#include "wx/splitter.h"

#include "pqwx_frame.h"
#include "pqwx_version.h"
#include "wx_flavour.h"
#include "object_browser.h"
#include "connect_dialogue.h"
#include "scripts_notebook.h"
#include "results_notebook.h"
#include "script_events.h"
#include "script_editor.h"

#if !defined(__WXMSW__) && !defined(__WXPM__)
    #include "pqwx-appicon.xpm"
#endif

DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptExecute)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptDisconnect)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ScriptReconnect)

BEGIN_EVENT_TABLE(PqwxFrame, wxFrame)
  EVT_MENU(wxID_EXIT, PqwxFrame::OnQuit)
  EVT_MENU(wxID_ABOUT, PqwxFrame::OnAbout)
  EVT_MENU(XRCID("ConnectObjectBrowser"), PqwxFrame::OnConnectObjectBrowser)
  EVT_MENU(XRCID("DisconnectObjectBrowser"), PqwxFrame::OnDisconnectObjectBrowser)
  EVT_MENU(XRCID("FindObject"), PqwxFrame::OnFindObject)
  EVT_MENU(XRCID("ExecuteScript"), PqwxFrame::OnExecuteScript)
  EVT_MENU(XRCID("DisconnectScript"), PqwxFrame::OnDisconnectScript)
  EVT_MENU(XRCID("ReconnectScript"), PqwxFrame::OnReconnectScript)
  EVT_MENU(wxID_NEW, PqwxFrame::OnNewScript)
  EVT_MENU(wxID_OPEN, PqwxFrame::OnOpenScript)
  EVT_CLOSE(PqwxFrame::OnCloseFrame)
  PQWX_SCRIPT_TO_WINDOW(wxID_ANY, PqwxFrame::OnScriptToWindow)
  PQWX_SCRIPT_SELECTED(wxID_ANY, PqwxFrame::OnScriptSelected)
  PQWX_OBJECT_SELECTED(wxID_ANY, PqwxFrame::OnObjectSelected)
  PQWX_SCRIPT_EXECUTION_BEGINNING(wxID_ANY, PqwxFrame::OnScriptExecutionBeginning)
  PQWX_SCRIPT_EXECUTION_FINISHING(wxID_ANY, PqwxFrame::OnScriptExecutionFinishing)
  PQWX_SCRIPT_NEW(wxID_ANY, PqwxFrame::OnScriptNew)
END_EVENT_TABLE()

const int TOOLBAR_MAIN = 500;

PqwxFrame::PqwxFrame(const wxString& title)
  : wxFrame(NULL, wxID_ANY, title), currentEditor(NULL), haveCurrentServer(false)
{
  SetIcon(wxICON(Pqwx_appicon));

  SetMenuBar(wxXmlResource::Get()->LoadMenuBar(_T("mainmenu")));

  CreateStatusBar(StatusBar_Fields);
  const int StatusBar_Widths[] = { -1, 120, 80, 80 };
  SetStatusWidths(sizeof(StatusBar_Widths)/sizeof(int), StatusBar_Widths);

  wxSplitterWindow *mainSplitter = new wxSplitterWindow(this, wxID_ANY);

  objectBrowser = new ObjectBrowser(mainSplitter, Pqwx_ObjectBrowser);
  editorSplitter = new wxSplitterWindow(mainSplitter, wxID_ANY);
  mainSplitter->SplitVertically(objectBrowser, editorSplitter);
  mainSplitter->SetSashGravity(0.2);
  mainSplitter->SetMinimumPaneSize(100);

  scriptsBook = new ScriptsNotebook(editorSplitter, Pqwx_ScriptsNotebook);
  resultsBook = new ResultsNotebook(editorSplitter, Pqwx_ResultsNotebook);
  editorSplitter->Initialize(scriptsBook);
  editorSplitter->SetMinimumPaneSize(100);
  editorSplitter->SetSashGravity(1.0);

  LoadFrameGeometry();

  mainSplitter->SetSashPosition(GetSize().GetWidth()/4);
}

void PqwxFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
  Close(true);
}

void PqwxFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
  wxAboutDialogInfo info;
  info.SetName(_T("PQWX"));
  info.SetVersion(_T(PQWX_VERSION));
  wxString description(_("PostgreSQL query tool"));
#ifdef __WXDEBUG__
  description << _(" - Debug build");
  description << _("\nlibpq ") << _T(PG_VERSION);
  description << _T("\n") << wxVERSION_STRING << _T(" ") << _T(WX_FLAVOUR);
#endif
  info.SetDescription(description);
  info.SetCopyright(_T("(c) 2011 Steve Haslam"));
  wxAboutBox(info);
}

void PqwxFrame::OnConnectObjectBrowser(wxCommandEvent& event)
{
  wxDialog *connect = new ConnectDialogue(NULL, new AddConnectionToObjectBrowser(this));
  connect->Show();
}

void PqwxFrame::OnDisconnectObjectBrowser(wxCommandEvent& event)
{
  objectBrowser->DisconnectSelected();
}

void PqwxFrame::OnFindObject(wxCommandEvent& event) {
  if (haveCurrentServer)
    objectBrowser->FindObject(currentServer, currentDatabase);
}

void PqwxFrame::OnCloseFrame(wxCloseEvent& event) {
  if (!IsFullScreen())
    SaveFrameGeometry();
  Destroy();
}

void PqwxFrame::LoadFrameGeometry() {
  wxConfigBase *cfg = wxConfig::Get();
  wxString geometry;
  if (cfg->Read(_T("/Geometry"), &geometry)) {
    wxRegEx pattern(_T("([0-9]+)x([0-9]+)\\+([0-9]+)\\+([0-9]+)"));
    if (pattern.Matches(geometry)) {
      wxLogDebug(_T("Restoring frame geometry: %s"), geometry.c_str());
      SetSize(wxSize(wxAtoi(pattern.GetMatch(geometry, 1)), wxAtoi(pattern.GetMatch(geometry, 2))));
      SetPosition(wxPoint(wxAtoi(pattern.GetMatch(geometry, 3)), wxAtoi(pattern.GetMatch(geometry, 4))));
      return;
    }
  }

  wxLogDebug(_T("Setting default frame geometry"));
  SetPosition(wxPoint(100,100));
  SetSize(wxSize(800,600));
}

void PqwxFrame::SaveFrameGeometry() const {
  wxConfigBase *cfg = wxConfig::Get();
  wxPoint pos = GetPosition();
  wxSize size = GetSize();
  cfg->Write(_T("/Geometry"), wxString::Format(_T("%dx%d+%d+%d"), size.GetWidth(), size.GetHeight(), pos.x, pos.y));
}

void PqwxFrame::OnNewScript(wxCommandEvent& event)
{
  bool suggest = haveCurrentServer;
  ServerConnection suggestServer;
  wxString suggestDatabase;
  if (suggest) {
    suggestServer = currentServer;
    suggestDatabase = currentDatabase;
  }

  ScriptEditor *editor = scriptsBook->OpenNewScript();
  if (suggest)
    editor->Connect(suggestServer, suggestDatabase);
  editor->SetFocus();
}

void PqwxFrame::OnScriptNew(PQWXDatabaseEvent& event)
{
  ScriptEditor *editor = scriptsBook->OpenNewScript();
  editor->Connect(event.GetServer(), event.GetDatabase());
  editor->SetFocus();
}

void PqwxFrame::OnOpenScript(wxCommandEvent& event)
{
  bool suggest = haveCurrentServer;
  ServerConnection suggestServer;
  wxString suggestDatabase;
  if (suggest) {
    suggestServer = currentServer;
    suggestDatabase = currentDatabase;
  }

  wxFileDialog dbox(this, _("Open File"), wxEmptyString, wxEmptyString,
		    _("SQL files (*.sql)|*.sql"));
  dbox.CentreOnParent();
  if (dbox.ShowModal() == wxID_OK) {
    ScriptEditor *editor = scriptsBook->OpenScriptFile(dbox.GetPath());
    if (suggest)
      editor->Connect(suggestServer, suggestDatabase);
    editor->SetFocus();
  }
}

void PqwxFrame::OnScriptToWindow(PQWXDatabaseEvent& event)
{
  ScriptEditor *editor = scriptsBook->OpenScriptWithText(event.GetString());
  editor->Connect(event.GetServer(), event.GetDatabase());
  editor->SetFocus();
}

void PqwxFrame::OnScriptSelected(PQWXDatabaseEvent &event)
{
  SetTitle(wxString::Format(_T("PQWX - %s"), event.GetString().c_str()));
  objectBrowser->UnmarkSelected();
  wxObject *obj = event.GetEventObject();
  ScriptEditor *editor = dynamic_cast<ScriptEditor*>(obj);
  wxASSERT(editor != NULL);
  currentEditor = editor;
  if (event.DatabaseSpecified()) {
    currentServer = event.GetServer();
    currentDatabase = event.GetDatabase();
    haveCurrentServer = true;
  }
  else {
    haveCurrentServer = false;
  }
  UpdateStatusBar(event);
}

void PqwxFrame::OnObjectSelected(PQWXDatabaseEvent &event)
{
  currentServer = event.GetServer();
  currentDatabase = event.GetDatabase();
  haveCurrentServer = true;
}

void PqwxFrame::UpdateStatusBar(const PQWXDatabaseEvent &event)
{
  GetStatusBar()->SetStatusText(event.GetServer().Identification(), StatusBar_Server);

  if (event.DatabaseSpecified())
    GetStatusBar()->SetStatusText(event.GetDatabase(), StatusBar_Database);
  else
    GetStatusBar()->SetStatusText(wxEmptyString, StatusBar_Database);

  if (!event.HasConnectionState()) {
    GetStatusBar()->SetStatusText(wxEmptyString, StatusBar_State);
  }
  else {
    switch (event.GetConnectionState()) {
    case Idle:
      GetStatusBar()->SetStatusText(wxEmptyString, StatusBar_State);
      break;
    case IdleInTransaction:
      GetStatusBar()->SetStatusText(_("TXN"), StatusBar_State);
      break;
    case TransactionAborted:
      GetStatusBar()->SetStatusText(_("ERROR"), StatusBar_State);
      break;
    case CopyToServer:
    case CopyToClient:
      GetStatusBar()->SetStatusText(_("COPY"), StatusBar_State);
      break;
    default:
      GetStatusBar()->SetStatusText(wxEmptyString, StatusBar_State);
    }
  }

  GetStatusBar()->SetStatusText(wxEmptyString, StatusBar_Message);
}

void PqwxFrame::OnExecuteScript(wxCommandEvent& event)
{
  wxASSERT(currentEditor != NULL);

  wxCommandEvent cmd(PQWX_ScriptExecute);
  currentEditor->ProcessEvent(cmd);
}

void PqwxFrame::OnDisconnectScript(wxCommandEvent &event) {
  wxASSERT(currentEditor != NULL);

  wxCommandEvent cmd(PQWX_ScriptDisconnect);
  currentEditor->ProcessEvent(cmd);
}

void PqwxFrame::OnReconnectScript(wxCommandEvent &event) {
  wxASSERT(currentEditor != NULL);

  wxCommandEvent cmd(PQWX_ScriptReconnect);
  currentEditor->ProcessEvent(cmd);
}

void PqwxFrame::OnScriptExecutionBeginning(wxCommandEvent &event)
{
  resultsBook->Reset();
  editorSplitter->SplitHorizontally(scriptsBook, resultsBook);
  scriptExecutionStopwatch.Start();
  event.SetEventObject(resultsBook);
}

void PqwxFrame::OnScriptExecutionFinishing(wxCommandEvent &event)
{
  long elapsed = scriptExecutionStopwatch.Time();
  GetStatusBar()->SetStatusText(wxString::Format(_("Finished in %.2lf seconds"), ((double) elapsed) / 1000.0), StatusBar_Message);
}
