/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __work_launcher_h
#define __work_launcher_h

/**
 * Allow dialogue box to perform work on the relevant database connection.
 */
class WorkLauncher {
public:
  virtual ~WorkLauncher() {}

  /**
   * Launch database work, sending completed/crashed events to dest.
   */
  virtual void DoWork(ObjectBrowserManagedWork *work) = 0;

  /**
   * Retrieve a reference to the database model.
   */
  virtual ObjectModelReference GetDatabaseRef() const = 0;

  /**
   * Retrieve the server connection details.
   */
  virtual ServerConnection GetServerConnection() const = 0;

  /**
   * Retrieve the database name.
   */
  virtual wxString GetDatabaseName() const = 0;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
