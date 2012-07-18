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

#include "wx/dialog.h"
#include "wx/ctrlsub.h"

#include "object_browser_managed_work.h"
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

  class QualifiedName {
  public:
    QualifiedName(const wxString& schema, const wxString& name) : schema(schema), name(name) {}
    wxString schema;
    wxString name;
  };

  template<typename T>
  class Mapper {
  public:
    T operator()(const QueryResults::Row&);
  };

  template<class T>
  class LoaderWork : public Work {
  public:
    LoaderWork(const ObjectModelReference& databaseRef, CreateDatabaseDialogue *dest, const wxString& queryName, WorkCompleted completionHandler) : Work(READ_ONLY, databaseRef, dest, completionHandler), queryName(queryName)
    {
      wxLogDebug(_T("%p: work to load things: %s"), this, queryName.c_str());
    }
    void DoManagedWork()
    {
      QueryResults rs = Query(queryName).List();
      result.reserve(rs.Rows().size());
      std::transform(rs.Rows().begin(), rs.Rows().end(), std::back_inserter(result), mapper);
    }
    std::vector<T> result;
  private:
    const wxString queryName;
    Mapper<T> mapper;
  };

  class ItemContainerAppender : public std::iterator<std::output_iterator_tag, void, void, void, void> {
  public:
    ItemContainerAppender(wxItemContainer* target) : target(target) {}
    ItemContainerAppender& operator=(const wxString& value)
    {
      target->Append(value);
      return *this;
    }
    ItemContainerAppender& operator=(const QualifiedName& qname)
    {
      if (qname.schema == _T("pg_catalog"))
        target->Append(qname.name);
      else
        target->Append(qname.schema + _T('.') + qname.name);
      return *this;
    }
    ItemContainerAppender& operator*() { return *this; }
    ItemContainerAppender& operator++() { return *this; }
    ItemContainerAppender& operator++(int) { return *this; }
  private:
    wxItemContainer* const target;
  };

  class ExecuteScriptWork : public Work {
  public:
    ExecuteScriptWork(const ObjectModelReference& databaseRef, CreateDatabaseDialogue *dest, const std::vector<wxString>& commands) : Work(NO_TRANSACTION, databaseRef, dest, &CreateDatabaseDialogue::OnScriptExecuted, &CreateDatabaseDialogue::OnScriptCrashed), commands(commands)
    {
      wxLogDebug(_T("%p: work to execute generated script"), this);
    }
    void DoManagedWork();
  private:
    const std::vector<wxString> commands;
  };

  static const SqlDictionary& GetSqlDictionary();
  static const std::vector<wxString>& GetPgEncodings();

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
