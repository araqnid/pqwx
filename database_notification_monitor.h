/**
 * @file
 * Database connection and worker thread declarations.
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __database_notification_monitor_h
#define __database_notification_monitor_h

#include "pqwx_config.h"

#ifdef PQWX_NOTIFICATION_MONITOR

#include <deque>
#include <list>
#include "libpq-fe.h"
#include "wx/thread.h"

/**
 * Listen for asynchronous notifications on database connections.
 */
class DatabaseNotificationMonitor {
public:
  class InputProcessor {
  public:
    virtual void operator()() = 0;
  };

  class Client {
  public:
    Client(int socketFd, InputProcessor *processor) : socketFd(socketFd), processor(processor) {}
    int GetFD() const { return socketFd; }
    void Invoke() const { (*processor)(); }
  private:
    int socketFd;
    InputProcessor *processor;
  };

  DatabaseNotificationMonitor() {}
  ~DatabaseNotificationMonitor() { EnsureStopped(); }

  void AddConnection(const Client& client);
  void RemoveConnection(int socketFd);
  void Quit();

private:
  class Work;

  class WorkerThread : public wxThread {
  public:
    WorkerThread() : wxThread(wxTHREAD_JOINABLE), running(false), quit(false) {}
    virtual ExitCode Entry();
    bool Running() const
    {
      wxCriticalSectionLocker locker(guardState);
      return running;
    }
  private:
    void MarkRunning()
    {
      wxCriticalSectionLocker locker(guardState);
      running = true;
    }
    void UnmarkRunning()
    {
      wxCriticalSectionLocker locker(guardState);
      running = false;
    }
    int MaxFd() const
    {
#ifdef HAVE_EVENTFD
      int max = controlFD;
#else
      int max = workerControlEndpoint;
#endif
      for (std::list<Client>::const_iterator iter = clients.begin(); iter != clients.end(); iter++) {
        int clientFd = (*iter).GetFD();
        if (clientFd > max) max = clientFd;
      }
      return max;
    }
    bool running;
    bool quit;
    mutable wxCriticalSection guardState;
    std::list<Client> clients;
#ifdef HAVE_EVENTFD
    int controlFD;
#else
    int workerControlEndpoint;
#endif
    std::deque<Work*> workQueue;
    wxCriticalSection guardWorkQueue;
    Work *ShiftWork()
    {
      wxCriticalSectionLocker locker(guardWorkQueue);
      if (workQueue.empty()) return NULL;
      Work *work = workQueue.front();
      workQueue.pop_front();
      return work;
    }
    void PushWork(Work *work)
    {
      wxCriticalSectionLocker locker(guardWorkQueue);
      workQueue.push_back(work);
    }
    friend class QuitWork;
    friend class TakeConnectionWork;
    friend class ReleaseConnectionWork;
    friend class DatabaseNotificationMonitor;
  };

  class Work {
  public:
    Work(WorkerThread *worker) : worker(worker) {}
    virtual void DoWork() = 0;
  protected:
    WorkerThread *worker;
  };

  class SynchronousWork : public Work {
  public:
    SynchronousWork(WorkerThread *worker) : Work(worker), done(false), condition(conditionMutex) {}
    void Done()
    {
      wxMutexLocker locker(conditionMutex);
      done = true;
      condition.Signal();
    }
    void Wait()
    {
      wxMutexLocker locker(conditionMutex);
      while (!done) {
        condition.Wait();
      }
    }
  private:
    bool done;
    wxMutex conditionMutex;
    wxCondition condition;
  };

  class TakeConnectionWork : public SynchronousWork {
  public:
    TakeConnectionWork(WorkerThread *worker, const Client &client) : SynchronousWork(worker), client(client) {}
    void DoWork();
  private:
    Client client;
  };

  class ReleaseConnectionWork : public SynchronousWork {
  public:
    ReleaseConnectionWork(WorkerThread *worker, int socketFd) : SynchronousWork(worker), socketFd(socketFd) {}
    void DoWork();
  private:
    int socketFd;
  };

  class QuitWork : public Work {
  public:
    QuitWork(WorkerThread *worker) : Work(worker) {}
    void DoWork();
  };

  WorkerThread worker;
#ifndef HAVE_EVENTFD
  int clientControlEndpoint;
#endif

  void EnsureRunning();
  void EnsureStopped();
  void Wake();

  // copying a monitor would be bad.
  DatabaseNotificationMonitor(const DatabaseNotificationMonitor&);
  DatabaseNotificationMonitor& operator=(const DatabaseNotificationMonitor&);

  friend class WorkerThread;
};

#endif

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
