/**
 * @file
 * Database interaction performed by the object browser model
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_database_work_h
#define __object_browser_database_work_h

#include "object_browser_managed_work.h"

class ObjectBrowserModel;
class ObjectBrowser;

/**
 * Unit of work to be performed on a database connection for the
 * object browser.
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

  ObjectBrowserWork(const ObjectModelReference& database, CompletionCallback* completion = NULL, TxMode txMode = READ_ONLY, const SqlDictionary &sqlDictionary = ObjectBrowserWork::GetSqlDictionary()) : ObjectBrowserManagedWork(txMode, database, sqlDictionary), completion(completion) {}
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
  virtual void UpdateModel(ObjectBrowserModel& model) = 0;

  /**
   * Update the object browser view to display the data fetched by this work.
   *
   * This method is executed on the GUI thread.
   */
  virtual void UpdateView(ObjectBrowser& browser) = 0;

  /**
   * Gets the default SQL dictionary for object browser work.
   */
  static const SqlDictionary& GetSqlDictionary();
private:
  CompletionCallback* const completion;
  friend class ObjectBrowserModel;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
