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
  /**
   * Merge data obtained by work object into object browser.
   *
   * This method is executed on the GUI thread.
   */
  virtual void LoadIntoView(ObjectBrowser *browser) = 0;
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
    wxCommandEvent event(PQWX_ObjectBrowserWorkFinished);
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
  void operator()() {
    owner->DoCommand(_T("SetupObjectBrowserConnection"));
  }
  void LoadIntoView(ObjectBrowser *ob) {
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
   * @param serverItem Server tree item to populate
   */
  RefreshDatabaseListWork(ServerModel *serverModel, wxTreeItemId serverItem) : serverModel(serverModel), serverItem(serverItem) {
    wxLogDebug(_T("%p: work to load database list"), this);
  }
protected:
  void operator()();
  void LoadIntoView(ObjectBrowser *ob);
private:
  ServerModel *serverModel;
  wxTreeItemId serverItem;
  wxString serverVersionString;
  int serverVersion;
  bool usingSSL;
  void ReadServer();
  void ReadDatabases();
  void ReadRoles();

  static bool CollateDatabases(DatabaseModel *d1, DatabaseModel *d2) {
    return d1->name < d2->name;
  }

  static bool CollateRoles(RoleModel *r1, RoleModel *r2) {
    return r1->name < r2->name;
  }
};

/**
 * Load schema members (relations and functions) for a database.
 */
class LoadDatabaseSchemaWork : public ObjectBrowserWork {
public:
  /**
   * Create work object.
   * @param databaseModel Database model to populate
   * @param databaseItem Database tree item to populate
   * @param expandAfter Expand tree item after populating
   */
  LoadDatabaseSchemaWork(DatabaseModel *databaseModel, wxTreeItemId databaseItem, bool expandAfter) : databaseModel(databaseModel), databaseItem(databaseItem), expandAfter(expandAfter) {
    wxLogDebug(_T("%p: work to load schema"), this);
  }
private:
  DatabaseModel *databaseModel;
  wxTreeItemId databaseItem;
  bool expandAfter;
protected:
  void operator()();
  void LoadRelations();
  void LoadFunctions();
  void LoadIntoView(ObjectBrowser *ob);
private:
  static inline bool IsSystemSchema(wxString schema) {
    return schema.StartsWith(_T("pg_")) || schema == _T("information_schema");
  }
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
  void LoadIntoView(ObjectBrowser *ob);
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
  void LoadIntoView(ObjectBrowser *ob);
};

/**
 * Load relation details from database
 */
class LoadRelationWork : public ObjectBrowserWork {
public:
  /**
   * @param relationModel Relation model to populate
   * @param relationItem Relation tree item to populate
   */
  LoadRelationWork(const RelationModel *relationModel, wxTreeItemId relationItem) : relationType(relationModel->type), oid(relationModel->oid), relationItem(relationItem) {
    wxLogDebug(_T("%p: work to load relation"), this);
  }
private:
  const RelationModel::Type relationType;
  const Oid oid;
  wxTreeItemId relationItem;
  RelationModel *detail;
  friend class ObjectBrowser;
private:
  void operator()();
  void ReadColumns();
  void ReadIndices();
  void ReadTriggers();
  void ReadSequences();
  void ReadConstraints();
  void LoadIntoView(ObjectBrowser *ob);
  static std::vector<int> ParseInt2Vector(const wxString &str);
};

#endif

// Local Variables:
// mode: c++
// End:
