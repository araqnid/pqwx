/**
 * @file
 * Database interaction managed by the object browser model
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_managed_work_h
#define __object_browser_managed_work_h

#include "wx/event.h"

#include "object_model_reference.h"
#include "sql_dictionary.h"
#include "query_results.h"
#include "database_work.h"

BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(PQWX_ObjectBrowserWorkFinished, -1)
  DECLARE_EVENT_TYPE(PQWX_ObjectBrowserWorkCrashed, -1)
END_DECLARE_EVENT_TYPES()

/**
 * Event issued when asynchronous database work finishes
 */
#define PQWX_OBJECT_BROWSER_WORK_FINISHED(id, fn) EVT_COMMAND(id, PQWX_ObjectBrowserWorkFinished, fn)

/**
 * Event issued when asynchronous database work throws an exception
 */
#define PQWX_OBJECT_BROWSER_WORK_CRASHED(id, fn) EVT_COMMAND(id, PQWX_ObjectBrowserWorkCrashed, fn)

/**
 * Unit of work that is managed by the object browser model.
 *
 * This might be triggered the object browser itself, or may be
 * executed by the object browser model on behalf of some dialogue.
 *
 * The object browser model will convert the completion of the work on
 * the database thread into wx events to be sent to the handler
 * specified when the work object is created.
 *
 * An instance of this class is wrapped by a DatabaseWork in order to
 * be executed. However, the DatabaseWork is deleted as soon as it has
 * completed, but the wrapper will pass this class back to object
 * browser as part of a "work finished" event so that is is available
 * in the GUI thread.
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


#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
