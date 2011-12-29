// -*- mode: c++ -*-

#ifndef __disconnect_work_h
#define __disconnect_work_h

class DisconnectWork : public DatabaseWork {
public:
  class CloseCallback {
  public:
    virtual void OnConnectionClosed() = 0;
  };

  DisconnectWork(CloseCallback *callback = NULL) : callback(callback) {}
  void Execute() {
    PQfinish(conn);
    db->LogDisconnect();
  }
private:
  CloseCallback *callback;
  void NotifyFinished() {
    if (callback)
      callback->OnConnectionClosed();
  }
};

#endif
