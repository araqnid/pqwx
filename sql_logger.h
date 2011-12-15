// -*- c++ -*-

#ifndef __sql_logger_h
#define __sql_logger_h

class SqlLogger {
public:
  virtual void LogSql(const char *sql) = 0;
  virtual void LogDisconnect() = 0;
};

#endif