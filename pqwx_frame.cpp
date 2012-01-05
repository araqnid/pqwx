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
#include "results_notebook.h"
#include "script_events.h"
#include "script_editor_pane.h"

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
  PQWX_DOCUMENT_SELECTED(wxID_ANY, PqwxFrame::OnDocumentSelected)
  PQWX_OBJECT_SELECTED(wxID_ANY, PqwxFrame::OnObjectSelected)
  PQWX_SCRIPT_NEW(wxID_ANY, PqwxFrame::OnScriptNew)
END_EVENT_TABLE()

const int TOOLBAR_MAIN = 500;

#ifndef __WXMSW__
static wxImage LoadVFSImage(const wxString &vfilename) {
  wxFileSystem fs;
  wxFSFile *fsfile = fs.OpenFile(vfilename);
  wxASSERT_MSG(fsfile != NULL, vfilename);
  wxImage im;
  wxInputStream *stream = fsfile->GetStream();
  im.LoadFile(*stream, fsfile->GetMimeType());
  delete fsfile;
  return im;
}

static wxIcon LoadVFSIcon(const wxString &vfilename) {
  wxIcon icon;
  icon.CopyFromBitmap(LoadVFSImage(vfilename));
  return icon;
}
#endif

PqwxFrame::PqwxFrame(const wxString& title)
  : wxFrame(NULL, wxID_ANY, title), currentEditor(NULL), haveCurrentServer(false)
{
#ifdef __WXMSW__
  SetIcon(wxIcon(_T("Pqwx_appicon")));
#else
  wxIconBundle icons;
  icons.AddIcon(LoadVFSIcon(_T("memory:PqwxFrame/pqwx-appicon-64.png")));
  icons.AddIcon(LoadVFSIcon(_T("memory:PqwxFrame/pqwx-appicon-48.png")));
  icons.AddIcon(LoadVFSIcon(_T("memory:PqwxFrame/pqwx-appicon-32.png")));
  icons.AddIcon(LoadVFSIcon(_T("memory:PqwxFrame/pqwx-appicon-16.png")));
  SetIcons(icons);
#endif

  SetMenuBar(wxXmlResource::Get()->LoadMenuBar(_T("mainmenu")));

  CreateStatusBar(StatusBar_Fields);
  const int StatusBar_Widths[] = { -1 };
  SetStatusWidths(sizeof(StatusBar_Widths)/sizeof(int), StatusBar_Widths);

  wxSplitterWindow *mainSplitter = new wxSplitterWindow(this, wxID_ANY);

  objectBrowser = new ObjectBrowser(mainSplitter, Pqwx_ObjectBrowser);
  documentsBook = new DocumentsNotebook(mainSplitter, Pqwx_DocumentsNotebook);
  mainSplitter->SplitVertically(objectBrowser, documentsBook);
  mainSplitter->SetSashGravity(0.2);
  mainSplitter->SetMinimumPaneSize(100);

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
#if PG_VERSION_NUM >= 90100
  const int pqVersion = PQlibVersion();
  description << _T("\nlibpq ") << (pqVersion/10000) << _T('.') << ((pqVersion/100)%100) << _T('.') << (pqVersion%100);
  if (pqVersion != PG_VERSION_NUM)
    description << _T(" (compiled against ") << _T(PG_VERSION) << _T(")");
#else
  description << _T("\nlibpq ") << _T(PG_VERSION);
#endif
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

  ScriptEditorPane *editor = documentsBook->OpenNewScript();
  if (suggest)
    editor->Connect(suggestServer, suggestDatabase);
  editor->SetFocus();
}

void PqwxFrame::OnScriptNew(PQWXDatabaseEvent& event)
{
  ScriptEditorPane *editor = documentsBook->OpenNewScript();
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
    ScriptEditorPane *editor = documentsBook->OpenNewScript();
    editor->OpenFile(dbox.GetPath());
    if (suggest)
      editor->Connect(suggestServer, suggestDatabase);
    editor->SetFocus();
  }
}

void PqwxFrame::OnScriptToWindow(PQWXDatabaseEvent& event)
{
  ScriptEditorPane *editor = documentsBook->OpenNewScript();
  editor->Populate(event.GetString());
  editor->Connect(event.GetServer(), event.GetDatabase());
  editor->SetFocus();
}

void PqwxFrame::OnDocumentSelected(PQWXDatabaseEvent &event)
{
  SetTitle(wxString::Format(_T("PQWX - %s"), event.GetString().c_str()));
  objectBrowser->UnmarkSelected();
  wxObject *obj = event.GetEventObject();
  currentEditorTarget = dynamic_cast<wxEvtHandler*>(obj);
  wxASSERT(currentEditorTarget != NULL);

  if (event.DatabaseSpecified()) {
    currentServer = event.GetServer();
    currentDatabase = event.GetDatabase();
    haveCurrentServer = true;
  }
  else {
    haveCurrentServer = false;
  }
}

void PqwxFrame::OnObjectSelected(PQWXDatabaseEvent &event)
{
  currentServer = event.GetServer();
  currentDatabase = event.GetDatabase();
  haveCurrentServer = true;
}

void PqwxFrame::OnExecuteScript(wxCommandEvent& event)
{
  wxASSERT(currentEditorTarget != NULL);

  wxCommandEvent cmd(PQWX_ScriptExecute);
  currentEditorTarget->ProcessEvent(cmd);
}

void PqwxFrame::OnDisconnectScript(wxCommandEvent &event) {
  wxASSERT(currentEditorTarget != NULL);

  wxCommandEvent cmd(PQWX_ScriptDisconnect);
  currentEditorTarget->ProcessEvent(cmd);
}

void PqwxFrame::OnReconnectScript(wxCommandEvent &event) {
  wxASSERT(currentEditorTarget != NULL);

  wxCommandEvent cmd(PQWX_ScriptReconnect);
  currentEditorTarget->ProcessEvent(cmd);
}
