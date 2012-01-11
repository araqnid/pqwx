#include <unistd.h>
#include <fcntl.h>
#include "wx/log.h"
#include "database_notification_monitor.h"

void DatabaseNotificationMonitor::AddConnection(const Client& client)
{
  EnsureRunning();
  TakeConnectionWork work(&worker, client);
  worker.PushWork(&work);
  Wake();
  work.Wait();
}

void DatabaseNotificationMonitor::RemoveConnection(int socketFd)
{
  wxASSERT(worker.Running());
  ReleaseConnectionWork work(&worker, socketFd);
  worker.PushWork(&work);
  Wake();
  work.Wait();
}

void DatabaseNotificationMonitor::EnsureRunning()
{
  wxCriticalSectionLocker locker(worker.guardState);
  if (worker.running) return;

  int endpoints[2];
  if (pipe2(endpoints, O_CLOEXEC) != 0) {
    wxLogSysError(_T("Failed to create pipe"));
    return;
  }

  worker.workerControlEndpoint = endpoints[0]; // read
  clientControlEndpoint = endpoints[1]; // write

  worker.Create();
  worker.Run();
}

void DatabaseNotificationMonitor::Quit()
{
  EnsureStopped();
}

void DatabaseNotificationMonitor::EnsureStopped()
{
  if (!worker.Running()) return;
  QuitWork work(&worker);
  worker.PushWork(&work);
  Wake();
  worker.Wait();
  worker.running = false;
  close(clientControlEndpoint);
  close(worker.workerControlEndpoint);
}

void DatabaseNotificationMonitor::Wake()
{
  const char *str = "!";
  write(clientControlEndpoint, str, 1);
}

wxThread::ExitCode DatabaseNotificationMonitor::WorkerThread::Entry()
{
  MarkRunning();

  fd_set readfds;
  
  while (!quit) {
    FD_ZERO(&readfds);

    FD_SET(workerControlEndpoint, &readfds);
    for (std::list<Client>::const_iterator iter = clients.begin(); iter != clients.end(); iter++) {
      int clientFd = (*iter).GetFD();
      FD_SET(clientFd, &readfds);
    }

    int rc = select(MaxFd() + 1, &readfds, NULL, NULL, NULL);
    if (rc < 0) {
      wxLogSysError(_T("select() failed"));
      break;
    }
    if (FD_ISSET(workerControlEndpoint, &readfds)) {
      char buffer[64];
      read(workerControlEndpoint, buffer, sizeof(buffer));
      Work *work;
      while ((work = ShiftWork()) != NULL) {
	(*work)();
      }
    }
    for (std::list<Client>::const_iterator iter = clients.begin(); iter != clients.end(); iter++) {
      int clientFd = (*iter).GetFD();
      if (FD_ISSET(clientFd, &readfds)) {
	(*iter).Invoke();
      }
    }
  }

  UnmarkRunning();

  return 0;
}

void DatabaseNotificationMonitor::TakeConnectionWork::operator()()
{
  worker->clients.push_back(client);
  Done();
}

void DatabaseNotificationMonitor::ReleaseConnectionWork::operator()()
{
  for (std::list<Client>::iterator iter = worker->clients.begin(); iter != worker->clients.end(); iter++) {
    if ((*iter).GetFD() == socketFd) {
      worker->clients.erase(iter);
      break;
    }
  }
  Done();
}

void DatabaseNotificationMonitor::QuitWork::operator()()
{
  worker->quit = true;
}
