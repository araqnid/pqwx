/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __create_database_dialogue_h
#define __create_database_dialogue_h

#include "wx/combobox.h"
#include "action_dialogue_work.h"

/**
 * Allow dialogue box to perform work on the relevant database connection.
 */
class WorkLauncher {
public:
  virtual ~WorkLauncher() {}

  /**
   * Launch database work, sending completed/crashed events to dest.
   */
  virtual void DoWork(ActionDialogueWork *work, wxEvtHandler *dest) = 0;

  /**
   * Retrieve the server connection details.
   */
  virtual ServerConnection GetServerConnection() const = 0;

  /**
   * Retrieve the database name.
   */
  virtual wxString GetDatabaseName() const = 0;
};

/**
 * Dialogue box to create a new database.
 */
class CreateDatabaseDialogue : public wxDialog {
public:
  /**
   * Called by the dialogue box after the script has been directly executed.
   */
  class ExecutionCallback {
  public:
    virtual ~ExecutionCallback(){}
    virtual void ActionPerformed() = 0;
  };

  CreateDatabaseDialogue(wxWindow *parent, WorkLauncher *launcher, ExecutionCallback *callback = NULL);
  ~CreateDatabaseDialogue();

private:
  WorkLauncher* const launcher;
  ExecutionCallback* const executionCallback;

  wxChoice *modeInput;
  wxTextCtrl *scriptInput;
  wxNotebook *tabsNotebook;

  wxTextCtrl *nameInput;
  wxComboBox *ownerInput;
  wxComboBox *encodingInput;
  wxComboBox *localeInput;
  wxComboBox *collationInput;
  wxCheckBox *collationOverrideInput;
  wxComboBox *ctypeInput;
  wxCheckBox *ctypeOverrideInput;

  static const int MODE_EXECUTE = 0;
  static const int MODE_WINDOW = 1;
  static const int MODE_FILE = 2;
  static const int MODE_CLIPBOARD = 3;

  class Work;

  typedef void (CreateDatabaseDialogue::*WorkCompleted)(Work*);

  class Work : public ActionDialogueWork {
  public:
    Work(TxMode txMode, WorkCompleted completionHandler) : ActionDialogueWork(txMode, GetSqlDictionary()), completionHandler(completionHandler) {}
    const WorkCompleted completionHandler;
  };

  class ListUsersWork : public Work {
  public:
    ListUsersWork() : Work(READ_ONLY, &CreateDatabaseDialogue::OnListUsersComplete)
    {
      wxLogDebug(_T("%p: work to list users for owners dropdown"), this);
    }
    void operator()();
    std::vector<wxString> result;
  };

  class ExecuteScriptWork : public Work {
  public:
    ExecuteScriptWork(const std::vector<wxString>& commands) : Work(NO_TRANSACTION, &CreateDatabaseDialogue::OnScriptExecuted), commands(commands)
    {
      wxLogDebug(_T("%p: work to execute generated script"), this);
    }
    void operator()();
  private:
    const std::vector<wxString> commands;
  };

  static const SqlDictionary& GetSqlDictionary();

  template<class T>
  void GenerateScript(T outputIterator);

  wxString QuoteLiteral(const wxString&);
  wxString QuoteIdent(const wxString&);

  void OnExecute(wxCommandEvent&);
  void OnCancel(wxCommandEvent&);
  void OnTabChanged(wxNotebookEvent&);
  void OnWorkFinished(wxCommandEvent&);
  void OnWorkCrashed(wxCommandEvent&);
  void OnListUsersComplete(Work*);
  void OnScriptExecuted(Work*);

  void InitXRC(wxWindow *parent);
  DECLARE_EVENT_TABLE();
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
