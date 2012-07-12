/**
 * @file
 * Database interaction managed by the object browser model
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_database_work_h
#define __object_browser_database_work_h

#include "object_browser.h"
#include "object_browser_model.h"
#include "query_results.h"
#include "database_work.h"

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
  virtual ~ObjectBrowserWork()
  {
    if (completion != NULL)
      delete completion;
  }

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

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
