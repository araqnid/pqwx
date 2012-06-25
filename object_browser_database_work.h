/**
 * @file
 * Database interaction performed by the object browser
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_database_work_h
#define __object_browser_database_work_h

#include "wx/msgdlg.h"
#include "object_browser.h"
#include "object_browser_model.h"
#include "query_results.h"
#include "database_work.h"
#include "script_events.h"

/**
 * Unit of work to be performed on a database connection for the
 * object browser.
 *
 * An instance of this class is wrapped by a DatabaseWork in order to
 * be executed. However, the DatabaseWork is deleted as soon as it has
 * completed, but the wrapper will pass this class back to object
 * browser as part of a "work finished" event so that is is available
 * in the GUI thread.
 */
class ObjectBrowserWork {
public:
  ObjectBrowserWork(const VersionedSql &sqlDictionary = ObjectBrowser::GetSqlDictionary()) : sqlDictionary(sqlDictionary) {}

  /**
   * Execute work.
   *
   * Similar to the same-named method in DatabaseWork. This method is
   * executed on the database worker thread.
   */
  virtual void operator()() = 0;
  /*
   * Merge the data fetched into the object model.
   *
   * This method is executed on the GUI thread.
   */
  virtual void UpdateModel(ObjectBrowserModel *model) = 0;
  /**
   * Update the object browser view to display the data fetched by this work.
   *
   * This method is executed on the GUI thread.
   */
  virtual void UpdateView(ObjectBrowser *browser) = 0;

  /**
   * If the database work threw a fatal exception, this will retrieve the message.
   */
  const wxString& GetCrashMessage() const { return crashMessage; }
protected:
  DatabaseWorkWithDictionary::NamedQueryExecutor Query(const wxString &name)
  {
    return owner->Query(name);
  }

  /**
   * The actual libpq connection object.
   */
  PGconn *conn;
public:
  /**
   * Quote an identified for use in a generated SQL statement.
   */
  wxString QuoteIdent(const wxString &value) { return owner->QuoteIdent(value); }
  /**
   * Quote an literal string for use in a generated SQL statement.
   */
  wxString QuoteLiteral(const wxString &value) { return owner->QuoteLiteral(value); }
  /**
   * The actual database work object wrapping this.
   */
  DatabaseWorkWithDictionary *owner;

private:
  const VersionedSql& sqlDictionary;
  wxString crashMessage;

  friend class ObjectBrowserDatabaseWork;
};

/**
 * Implementation of DatabaseWork that deals with passing results back to the GUI thread.
 */
class ObjectBrowserDatabaseWork : public DatabaseWorkWithDictionary {
public:
  /**
   * Create work object.
   */
  ObjectBrowserDatabaseWork(wxEvtHandler *dest, ObjectBrowserWork *work) : DatabaseWorkWithDictionary(work->sqlDictionary), dest(dest), work(work) {}
  void operator()() {
    TransactionBoundary txn(this);
    work->owner = this;
    work->conn = conn;
    (*work)();
  }
  void NotifyFinished() {
    wxLogDebug(_T("%p: object browser work finished, notifying GUI thread"), work);
    wxCommandEvent event(PQWX_ObjectBrowserWorkFinished);
    event.SetClientData(work);
    dest->AddPendingEvent(event);
  }
  void NotifyCrashed() {
    wxLogDebug(_T("%p: object browser work crashed with no known exception, notifying GUI thread"), work);
    wxCommandEvent event(PQWX_ObjectBrowserWorkCrashed);
    event.SetClientData(work);
    dest->AddPendingEvent(event);
  }
  void NotifyCrashed(const std::exception& e) {
    wxLogDebug(_T("%p: object browser work crashed with some exception, notifying GUI thread"), work);

    const PgQueryRelatedException* query = dynamic_cast<const PgQueryRelatedException*>(&e);
    if (query != NULL) {
      if (query->IsFromNamedQuery())
        work->crashMessage = _T("While running query \"") + query->GetQueryName() + _T("\":\n");
      else
        work->crashMessage = _T("While running dynamic SQL:\n\n") + query->GetSql() + _T("\n\n");
      const PgQueryFailure* queryError = dynamic_cast<const PgQueryFailure*>(&e);
      if (queryError != NULL) {
        const PgError& details = queryError->GetDetails();
        work->crashMessage += details.GetSeverity() + _T(": ") + details.GetPrimary();
        if (!details.GetDetail().empty())
          work->crashMessage += _T("\nDETAIL: ") + details.GetDetail();
        if (!details.GetHint().empty())
          work->crashMessage += _T("\bHINT: ") + details.GetDetail();
        for (std::vector<wxString>::const_iterator iter = details.GetContext().begin(); iter != details.GetContext().end(); iter++) {
          work->crashMessage += _T("\nCONTEXT: ") + *iter;
        }
      }
      else
        work->crashMessage += wxString(e.what(), wxConvUTF8);
    }
    else {
        work->crashMessage = wxString(e.what(), wxConvUTF8);
    }

    wxCommandEvent event(PQWX_ObjectBrowserWorkCrashed);
    event.SetClientData(work);
    dest->AddPendingEvent(event);
  }

private:
  wxEvtHandler *dest;
  ObjectBrowserWork *work;

  class TransactionBoundary {
  public:
    TransactionBoundary(DatabaseWork *work) : work(work), began(FALSE)
    {
      work->DoCommand("BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY");
      began = TRUE;
    }
    ~TransactionBoundary()
    {
      if (began) {
        try {
          work->DoCommand("END");
        } catch (...) {
          // ignore exceptions trying to end transaction
        }
      }
    }
  private:
    DatabaseWork *work;
    bool began;
  };
};

/**
 * Initialise database connection.
 *
 * Run once on each database connection added to the object browser.
 */
class SetupDatabaseConnectionWork : public ObjectBrowserWork {
protected:
  void operator()()
  {
    owner->DoCommand(_T("SetupObjectBrowserConnection"));
  }
  void UpdateModel(ObjectBrowserModel *model)
  {
  }
  void UpdateView(ObjectBrowser *ob)
  {
  }
};

/**
 * Load server information, database list and role list from server.
 */
class RefreshDatabaseListWork : public ObjectBrowserWork {
public:
  /**
   * Create work object
   * @param serverModel Server model to populate
   */
  RefreshDatabaseListWork(const wxString& serverId) : serverId(serverId)
  {
    wxLogDebug(_T("%p: work to load database list"), this);
  }
protected:
  void operator()();
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
private:
  const wxString serverId;
  wxString serverVersionString;
  int serverVersion;
  SSL *ssl;
  std::vector<DatabaseModel> databases;
  std::vector<RoleModel> roles;
  std::vector<TablespaceModel> tablespaces;
  void ReadServer();
  void ReadDatabases();
  void ReadRoles();
  void ReadTablespaces();
};

/**
 * Load schema members (relations and functions) for a database.
 */
class LoadDatabaseSchemaWork : public ObjectBrowserWork {
public:
  /**
   * Create work object.
   * @param databaseModel Database model to populate
   * @param expandAfter Expand tree item after populating
   */
  LoadDatabaseSchemaWork(const ObjectModelReference& databaseRef, bool expandAfter) : databaseRef(databaseRef), expandAfter(expandAfter) {
    wxLogDebug(_T("%p: work to load schema"), this);
  }
private:
  const ObjectModelReference databaseRef;
  const bool expandAfter;
  DatabaseModel incoming;
protected:
  void operator()();
  void LoadRelations();
  void LoadFunctions();
  void LoadTextSearchDictionaries();
  void LoadTextSearchTemplates();
  void LoadTextSearchParsers();
  void LoadTextSearchConfigurations();
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
private:
  static const std::map<wxString, RelationModel::Type> relationTypeMap;
  static const std::map<wxString, FunctionModel::Type> functionTypeMap;
};

/**
 * Load descriptions of objects from database.
 */
class LoadDatabaseDescriptionsWork : public ObjectBrowserWork {
public:
  /**
   * @param databaseModel Database model to populate
   */
  LoadDatabaseDescriptionsWork(DatabaseModel *databaseModel) : databaseModel(databaseModel) {
    wxLogDebug(_T("%p: work to load schema object descriptions"), this);
  }
private:
  DatabaseModel *databaseModel;
  std::map<unsigned long, wxString> descriptions;
protected:
  void operator()();
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
};

/**
 * Callback interface to notify some client that the schema index has been built.
 */
class IndexSchemaCompletionCallback {
public:
  /**
   * Called when schema index completed.
   */
  virtual void Completed(ObjectBrowser *ob, DatabaseModel *db, const CatalogueIndex *index) = 0;
};

/**
 * Build index of database schema for object finder.
 */
class IndexDatabaseSchemaWork : public ObjectBrowserWork {
public:
  /**
   * @param database Database being indexed
   * @param completion Additional callback to notify when indexing completed
   */
  IndexDatabaseSchemaWork(DatabaseModel *database, IndexSchemaCompletionCallback *completion = NULL) : database(database), completion(completion) {
    wxLogDebug(_T("%p: work to index schema"), this);
  }
private:
  DatabaseModel *database;
  IndexSchemaCompletionCallback *completion;
  CatalogueIndex *catalogueIndex;
  static const std::map<wxString, CatalogueIndex::Type> typeMap;
protected:
  void operator()();
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
};

/**
 * Load relation details from database
 */
class LoadRelationWork : public ObjectBrowserWork {
public:
  /**
   * @param relationModel Relation model to populate
   */
  LoadRelationWork(RelationModel::Type relationType, const ObjectModelReference& relationRef) : relationType(relationType), relationRef(relationRef) {
    wxLogDebug(_T("%p: work to load relation"), this);
  }
private:
  const RelationModel::Type relationType;
  const ObjectModelReference relationRef;
  RelationModel incoming;
private:
  void operator()();
  void ReadColumns();
  void ReadIndices();
  void ReadTriggers();
  void ReadSequences();
  void ReadConstraints();
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
  static std::vector<int> ParseInt2Vector(const wxString &str);
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
