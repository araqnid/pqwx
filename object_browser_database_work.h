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
 * Unit of work that is managed by the object browser model.
 *
 * This might be triggered the object browser itself, or may be
 * executed by the object browser model on behalf of some dialogue.
 */
class ObjectBrowserManagedWork {
public:
  /**
   * Mode for wrapping a transaction around the work.
   */
  enum TxMode {
    /**
     * Work is performed in a read-only serializable snapshot.
     */
    READ_ONLY,
    /**
     * Work is performed in a single read-committed transaction.
     */
    TRANSACTIONAL,
    /**
     * No transaction wrapping.
     */
    NO_TRANSACTION
  };

  ObjectBrowserManagedWork(TxMode txMode, const ObjectModelReference& database, const SqlDictionary& sqlDictionary, wxEvtHandler* dest = NULL) : txMode(txMode), database(database), sqlDictionary(sqlDictionary), dest(dest) {}
  virtual ~ObjectBrowserManagedWork() {}

  /**
   * Execute work.
   *
   * Similar to the same-named method in DatabaseWork. This method is
   * executed on the database worker thread.
   */
  virtual void operator()() = 0;
  /**
   * If the database work threw a fatal exception, this will retrieve the message.
   */
  const wxString& GetCrashMessage() const { return crashMessage; }

  /**
   * Quote an identified for use in a generated SQL statement.
   */
  wxString QuoteIdent(const wxString &value) { return owner->QuoteIdent(value); }
  /**
   * Quote an literal string for use in a generated SQL statement.
   */
  wxString QuoteLiteral(const wxString &value) { return owner->QuoteLiteral(value); }
protected:
  DatabaseWorkWithDictionary::NamedQueryExecutor Query(const wxString &name)
  {
    return owner->Query(name);
  }

  /**
   * The actual libpq connection object.
   */
  PGconn *conn;
  /**
   * The actual database work object wrapping this.
   */
  DatabaseWorkWithDictionary *owner;

private:
  const TxMode txMode;
  const ObjectModelReference database;
  const SqlDictionary& sqlDictionary;
  wxEvtHandler* const dest;

  wxString crashMessage;
  friend class ObjectBrowserDatabaseWork;
  friend class ObjectBrowserModel;
};

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
class ObjectBrowserWork : public ObjectBrowserManagedWork {
public:
  /**
   * A callback object called after the database work has completed.
   *
   * This is called after the work completed successfully, and the views have been updated, or after the work crashed.
   */
  class CompletionCallback {
  public:
    virtual ~CompletionCallback() {}

    /**
     * Called on completion.
     */
    virtual void OnCompletion() = 0;

    /**
     * Called after crashing.
     */
    virtual void OnCrash() {}
  };

  ObjectBrowserWork(const ObjectModelReference& database, CompletionCallback* completion = NULL, const SqlDictionary &sqlDictionary = ObjectBrowser::GetSqlDictionary()) : ObjectBrowserManagedWork(READ_ONLY, database, sqlDictionary), completion(completion) {}
  virtual ~ObjectBrowserWork() {}

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

private:
  CompletionCallback* const completion;
  friend class ObjectBrowserModel;
};

/**
 * Initialise database connection.
 *
 * Run once on each database connection added to the object browser.
 */
class SetupDatabaseConnectionWork : public ObjectBrowserWork {
public:
  SetupDatabaseConnectionWork(const ObjectModelReference& databaseRef) : ObjectBrowserWork(databaseRef) {}
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
  RefreshDatabaseListWork(const ObjectModelReference& database) : ObjectBrowserWork(database), serverId(database.GetServerId())
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
  LoadDatabaseSchemaWork(const ObjectModelReference& databaseRef, bool expandAfter) : ObjectBrowserWork(databaseRef), databaseRef(databaseRef), expandAfter(expandAfter) {
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
  template<typename T>
  void LoadSimpleSchemaMembers(const wxString &queryName, typename std::vector<T>& vec);
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
  LoadDatabaseDescriptionsWork(const ObjectModelReference& databaseRef) : ObjectBrowserWork(databaseRef), databaseRef(databaseRef) {
    wxLogDebug(_T("%p: work to load schema object descriptions"), this);
  }
private:
  const ObjectModelReference databaseRef;
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
  virtual ~IndexSchemaCompletionCallback() {}
  /**
   * Called when schema index completed.
   */
  virtual void Completed(ObjectBrowser *ob, const ObjectModelReference& databaseRef, const CatalogueIndex *index) = 0;
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
  IndexDatabaseSchemaWork(const ObjectModelReference& databaseRef, IndexSchemaCompletionCallback *completion = NULL) : ObjectBrowserWork(databaseRef), databaseRef(databaseRef), completion(completion) {
    wxLogDebug(_T("%p: work to index schema"), this);
  }
private:
  const ObjectModelReference databaseRef;
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
  LoadRelationWork(RelationModel::Type relationType, const ObjectModelReference& relationRef) : ObjectBrowserWork(relationRef.DatabaseRef()), relationType(relationType), relationRef(relationRef) {
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
