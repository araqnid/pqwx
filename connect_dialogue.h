/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __connect_dialogue_h
#define __connect_dialogue_h

#include <list>
#include "wx/xrc/xmlres.h"
#include "object_browser.h"
#ifdef USE_DEBIAN_PGCLUSTER
#include "debian_pgcluster.h"
#endif

BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(PQWX_ConnectionAttemptCompleted, -1)
END_DECLARE_EVENT_TYPES()

#define PQWX_CONNECTION_ATTEMPT_COMPLETED(id, fn) EVT_COMMAND(id, PQWX_ConnectionAttemptCompleted, fn)

/**
 * Dialogue box to initiate a database connection.
 *
 * Basically, this is a box to edit a ServerConnection, with
 * functionality to actually make that into a real DatabaseConnection.
 */
class ConnectDialogue : public wxDialog {
public:
  /**
   * Callback interface for non-modal operation of the connection
   * dialogue.
   *
   * When used non-modally, an callback implementing this interface is
   * supplied to be notified when the connection is established, or if
   * the user gives up and cancels.
   *
   * Implementations <b>must</b> be allocated on the heap, as they
   * will be deleted just after one of the callback methods is called.
   */
  class CompletionCallback {
  public:
    virtual ~CompletionCallback() { }
    /**
     * Connection established.
     *
     * @param server Server connection properties
     * @param db Connection to administrative database
     */
    virtual void Connected(const ServerConnection &server, DatabaseConnection *db) = 0;
    /**
     * Connection cancelled.
     */
    virtual void Cancelled() = 0;
  };

  /**
   * Create connection dialogue.
   */
  ConnectDialogue(wxWindow *parent, CompletionCallback *callback = NULL)
    : wxDialog(), callback(callback), recentServersConfigPath(_T("RecentServers")) {
    InitXRC(parent);
    LoadRecentServers();
    connection = NULL;
    cancelling = false;
  }

  /**
   * Suggest initial server connection properties.
   *
   * Maybe this should be a constructor parameter instead.
   */
  void Suggest(const ServerConnection &conninfo);

  /**
   * Immediately try a connection with the given server properties.
   */
  void DoInitialConnection(const ServerConnection &conninfo) { DoInitialConnection(conninfo, conninfo.globalDbName); }

  /**
   * Immediately try a connection to the specified database.
   */
  void DoInitialConnection(const ServerConnection &conninfo, const wxString &dbname);

  /**
   * Gets the server parameters used to make a connection.
   *
   * This can be called when using the connection dialogue modally.
   */
  const ServerConnection& GetServerParameters() const { return server; }

  /**
   * Gets the database connection.
   *
   * This can be called when using the connection dialogue modally.
   */
  DatabaseConnection* GetConnection() const { return db; }
private:
  class ConnectionWork : public ConnectionCallback {
  public:
    ConnectionWork(ConnectDialogue *owner, const ServerConnection &server, DatabaseConnection *db) : owner(owner), server(server), db(db) { }
    virtual ~ConnectionWork() { }
    void OnConnection(bool usedPassword_) {
      state = CONNECTED;
      usedPassword = usedPassword_;
      notifyFinished();
    }
    void OnConnectionFailed(const PgError& error_) {
      state = FAILED;
      error = error_;
      notifyFinished();
    }
    void OnConnectionNeedsPassword() {
      state = NEEDS_PASSWORD;
      notifyFinished();
    }
    enum { CONNECTED, FAILED, NEEDS_PASSWORD } state;
    PgError error;
  private:
    void notifyFinished() {
      wxCommandEvent event(PQWX_ConnectionAttemptCompleted);
      event.SetClientData(this);
      owner->AddPendingEvent(event);
    }
    ConnectDialogue *owner;
    ServerConnection server;
    DatabaseConnection *db;
    bool usedPassword;
    friend class ConnectDialogue;
  };

  wxComboBox *hostnameInput;
  wxComboBox *usernameInput;
  wxTextCtrl *passwordInput;
  wxCheckBox *savePasswordInput;
  wxButton *okButton;
  wxButton *cancelButton;

  void OnConnect(wxCommandEvent& event) {
    StartConnection(server.globalDbName);
  }
  void OnCancel(wxCommandEvent& event);
  void OnRecentServerChosen(wxCommandEvent& event);
  void OnConnectionFinished(wxCommandEvent& event);

  static const unsigned maxRecentServers = 10;
  CompletionCallback *callback;
  void InitXRC(wxWindow *parent) {
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("connect"));
    hostnameInput = XRCCTRL(*this, "hostname_value", wxComboBox);
    usernameInput = XRCCTRL(*this, "username_value", wxComboBox);
    passwordInput = XRCCTRL(*this, "password_value", wxTextCtrl);
    savePasswordInput = XRCCTRL(*this, "save_password_value", wxCheckBox);
    okButton = XRCCTRL(*this, "wxID_OK", wxButton);
    cancelButton = XRCCTRL(*this, "wxID_CANCEL", wxButton);
  }
  void StartConnection(const wxString &dbname);
  void MarkBusy();
  void UnmarkBusy();
  ConnectionWork *connection;
  bool cancelling;
  ServerConnection server;
  DatabaseConnection *db;
#ifdef USE_DEBIAN_PGCLUSTER
  PgClusters clusters;
#endif

  class RecentServerParameters {
    wxString server;
    wxString username;
    wxString password;
    friend class ConnectDialogue;
  public:
    bool operator==(const RecentServerParameters &other) const {
      return server == other.server;
    }
    bool operator==(const wxString &otherName) const {
      return server == otherName;
    }
  };
  std::list<RecentServerParameters> recentServerList;
  const wxString recentServersConfigPath;
  // read/write just transfers recentServerList (model) to configuration file
  void ReadRecentServers();
  void WriteRecentServers();
  // load/save calls read/write and moves data from recentServerList to the UI and back
  void LoadRecentServers();
  void LoadRecentServer(const RecentServerParameters&);
  void SaveRecentServers();
  void SetupDefaults();

  DECLARE_EVENT_TABLE();
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
