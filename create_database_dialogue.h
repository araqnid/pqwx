/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __create_database_dialogue_h
#define __create_database_dialogue_h

class wxNotebook;
class wxChoice;
class wxCheckBox;
class wxComboBox;
class wxNotebookEvent;

#include "object_browser_database_work.h"
#include "work_launcher.h"

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
  wxComboBox *templateInput;
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

  class Work : public ObjectBrowserManagedWork {
  public:
    Work(TxMode txMode, const ObjectModelReference& databaseRef, CreateDatabaseDialogue* dest, WorkCompleted completionHandler, WorkCompleted crashHandler = NULL) : ObjectBrowserManagedWork(txMode, databaseRef, GetSqlDictionary(), dest), completionHandler(completionHandler), crashHandler(crashHandler) {}
    const WorkCompleted completionHandler;
    const WorkCompleted crashHandler;
  };

  class ListUsersWork : public Work {
  public:
    ListUsersWork(const ObjectModelReference& databaseRef, CreateDatabaseDialogue *dest) : Work(READ_ONLY, databaseRef, dest, &CreateDatabaseDialogue::OnListUsersComplete)
    {
      wxLogDebug(_T("%p: work to list users for owners dropdown"), this);
    }
    void operator()();
    std::vector<wxString> result;
  };

  class ListTemplatesWork : public Work {
  public:
    ListTemplatesWork(const ObjectModelReference& databaseRef, CreateDatabaseDialogue *dest) : Work(READ_ONLY, databaseRef, dest, &CreateDatabaseDialogue::OnListTemplatesComplete)
    {
      wxLogDebug(_T("%p: work to list templates for dropdown"), this);
    }
    void operator()();
    std::vector<wxString> result;
  };

  class QualifiedName {
  public:
    QualifiedName(const wxString& schema, const wxString& name) : schema(schema), name(name) {}
    wxString schema;
    wxString name;
  };

  class ListCollationsWork : public Work {
  public:
    ListCollationsWork(const ObjectModelReference& databaseRef, CreateDatabaseDialogue *dest) : Work(READ_ONLY, databaseRef, dest, &CreateDatabaseDialogue::OnListCollationsComplete)
    {
      wxLogDebug(_T("%p: work to list collations for dropdown"), this);
    }
    void operator()();
    std::vector<QualifiedName> result;
  };

  class ExecuteScriptWork : public Work {
  public:
    ExecuteScriptWork(const ObjectModelReference& databaseRef, CreateDatabaseDialogue *dest, const std::vector<wxString>& commands) : Work(NO_TRANSACTION, databaseRef, dest, &CreateDatabaseDialogue::OnScriptExecuted, &CreateDatabaseDialogue::OnScriptCrashed), commands(commands)
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
  void OnListTemplatesComplete(Work*);
  void OnListCollationsComplete(Work*);
  void OnScriptExecuted(Work*);
  void OnScriptCrashed(Work*);

  void InitXRC(wxWindow *parent);
  DECLARE_EVENT_TABLE();
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
