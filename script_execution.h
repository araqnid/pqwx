/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __script_execution_h
#define __script_execution_h

#include <map>
#include "execution_lexer.h"
#include "script_query_work.h"

class ScriptEditorPane;

/**
 * Responsible for executing a script on a database connection.
 */
class ScriptExecution {
public:
  ScriptExecution(ScriptEditorPane *owner, wxCharBuffer buffer, unsigned length) :
    owner(owner), buffer(buffer),
    queryBuffer(this->buffer.data()),
    lexer(this->buffer.data(), length),
    lastSqlPosition(0), queryBufferExecuted(false),
    rowsRetrieved(0), errorsEncountered(0)
  {
    stopwatch.Start();
  }

  bool EncounteredErrors() const { return errorsEncountered > 0; }
  unsigned TotalRows() const { return rowsRetrieved; }
  long ElapsedTime() const { return stopwatch.Time(); }

  /**
   * Work through the script buffer, and dispatch work to database connection as applicable.
   */
  void Proceed()
  {
    NextState nextState;
    do {
      nextState = ProcessExecution();
      if (nextState == Finish) {
        FinishExecution();
      }
    } while (nextState == NeedMore);
  }

  /**
   * Process result of a query performed by a database thread.
   */
  void ProcessQueryResult(ScriptQueryWork::Result*);

  /**
   * Process notice message received while running query.
   */
  void ProcessConnectionNotice(const PgError& error);

private:
  class QueryBuffer {
  public:
    QueryBuffer(const char *buffer) : buffer(buffer) {}
    bool empty() const
    {
      return tokens.empty() || !NonEmptyTokenExists();
    }
    void clear()
    {
      tokens.clear();
    }
    void operator=(const ExecutionLexer::Token& t)
    {
      tokens.clear();
      tokens.push_back(t);
    }
    void operator+=(const ExecutionLexer::Token& t)
    {
      tokens.push_back(t);
    }
    unsigned length() const
    {
      return std::for_each(tokens.begin(), tokens.end(), AccumulateLength()).length;
    }
    operator std::string()
    {
      return std::for_each(tokens.begin(), tokens.end(), AccumulateContent(buffer, length())).str;
    }
    operator wxString()
    {
      std::string str = *this;
      return wxString(str.c_str(), wxConvUTF8);
    }

  private:
    const char * const buffer;
    std::vector<ExecutionLexer::Token> tokens;
    static bool TokenIsNonEmpty(const ExecutionLexer::Token& t)
    {
      return t.length > 0;
    }
    class AccumulateLength {
    public:
      AccumulateLength() : length(0) {}
      void operator()(const ExecutionLexer::Token& t)
      {
        length += t.length;
      }
      unsigned length;
    };
    class AccumulateContent {
    public:
      AccumulateContent(const char* buffer) : buffer(buffer) {}
      AccumulateContent(const char* buffer, unsigned length) : buffer(buffer)
      {
        str.reserve(length);
      }
      void operator()(const ExecutionLexer::Token& t)
      {
        str.append(buffer + t.offset, t.length);
      }
      std::string str;
    private:
      const char* const buffer;
    };
    bool NonEmptyTokenExists() const
    {
      return std::find_if(tokens.begin(), tokens.end(), TokenIsNonEmpty) != tokens.end();
    }
  };

  ScriptEditorPane *owner;
  wxCharBuffer buffer;
  QueryBuffer queryBuffer;
  ExecutionLexer lexer;
  unsigned lastSqlPosition;
  bool queryBufferExecuted;
  unsigned rowsRetrieved, errorsEncountered;
  wxStopWatch stopwatch;

  enum NextState {
    NeedMore,
    NoMore,
    Finish
  };

  typedef NextState (ScriptExecution::*PsqlCommandHandler)(const wxString&, const ExecutionLexer::Token&);
  static const std::map<wxString, PsqlCommandHandler> psqlCommandHandlers;
  static std::map<wxString, PsqlCommandHandler> InitPsqlCommandHandlers();

  NextState PsqlChangeDatabase(const wxString &args, const ExecutionLexer::Token &t);
  NextState PsqlExecuteQueryBuffer(const wxString &args, const ExecutionLexer::Token &t);
  NextState PsqlPrintQueryBuffer(const wxString &args, const ExecutionLexer::Token &t);
  NextState PsqlResetQueryBuffer(const wxString &args, const ExecutionLexer::Token &t);
  NextState PsqlPrintMessage(const wxString &args, const ExecutionLexer::Token &t);
  NextState PsqlQuitExecution(const wxString &args, const ExecutionLexer::Token &t);

  NextState ProcessExecution();
  void FinishExecution();
  void BeginQuery();
  void BeginPutCopyData();

  void ReportInternalError(const wxString &error, const wxString &command, unsigned scriptPosition);

  wxString GetWXString(const ExecutionLexer::Token &token) const { return lexer.GetWXString(token); }

  ExecutionLexer::Token NextToken() { return lexer.Pull(); }
  ExecutionLexer::Token CopyDataToken() { return lexer.ReadCopyData(); }

  void AppendSql(const ExecutionLexer::Token& token)
  {
    if (queryBuffer.empty() || queryBufferExecuted) {
      queryBufferExecuted = false;
      queryBuffer = token;
      lastSqlPosition = token.offset;
    }
    else {
      queryBuffer += token;
    }
  }

  char CharAt(unsigned offset) const { return buffer.data()[offset]; }
  void BumpErrors() { ++errorsEncountered; }
  void AddRows(unsigned rows) { rowsRetrieved += rows; }
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
