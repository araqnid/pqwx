#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "script_execution.h"
#include "script_editor_pane.h"
#include "script_query_work.h"
#include "results_notebook.h"

std::map<wxString, ScriptExecution::PsqlCommandHandler> ScriptExecution::InitPsqlCommandHandlers()
{
  std::map<wxString, PsqlCommandHandler> handlers;
  handlers[_T("c")] = &ScriptExecution::PsqlChangeDatabase;
  handlers[_T("connect")] = &ScriptExecution::PsqlChangeDatabase;
  handlers[_T("g")] = &ScriptExecution::PsqlExecuteQueryBuffer;
  handlers[_T("echo")] = &ScriptExecution::PsqlPrintMessage;
  handlers[_T("p")] = &ScriptExecution::PsqlPrintQueryBuffer;
  handlers[_T("r")] = &ScriptExecution::PsqlResetQueryBuffer;
  handlers[_T("quit")] = &ScriptExecution::PsqlQuitExecution;
  return handlers;
}

const std::map<wxString, ScriptExecution::PsqlCommandHandler> ScriptExecution::psqlCommandHandlers = InitPsqlCommandHandlers();

#define CALL_PSQL_HANDLER(editor, handler) ((editor).*(handler))

inline void ScriptExecution::ReportInternalError(const wxString &error, const wxString &command, unsigned scriptPosition)
{
  owner->GetOrCreateResultsBook()->ScriptInternalError(error, scriptPosition);
}

ScriptExecution::NextState ScriptExecution::ProcessExecution()
{
  if (owner->state == CopyToServer) {
    BeginPutCopyData();
    return NoMore;
  }

  ExecutionLexer::Token t = NextToken();
  if (t.type == ExecutionLexer::Token::END) {
    if (!queryBuffer.empty() && !queryBufferExecuted) {
      BeginQuery();
      return NoMore;
    }
    else
      return Finish;
  }

  if (t.type == ExecutionLexer::Token::SQL) {
    AppendSql(t);

    for (unsigned ofs = t.length - 1; ofs >= 0; ofs--) {
      char c = CharAt(t.offset + ofs);
      if (c == ';') {
        // execute immediately
        BeginQuery();
        return NoMore;
      }
      else if (!isspace(c)) {
        break;
      }
    }

    // look for following psql command or end of input
    return NeedMore;
  }
  else {
    wxString fullCommandString = GetWXString(t);
    wxString command;
    wxString parameters;
    wxStringTokenizer tkz(fullCommandString, _T(" \r\n\t"));

    wxASSERT(tkz.HasMoreTokens());
    command = tkz.GetNextToken().Mid(1).Lower();
    if (tkz.HasMoreTokens()) {
      parameters = fullCommandString.Mid(tkz.GetPosition()).Trim();
    }

    wxLogDebug(_T("psql | %s | %s"), command.c_str(), parameters.c_str());
    std::map<wxString, PsqlCommandHandler>::const_iterator handler = psqlCommandHandlers.find(command);
    if (handler == psqlCommandHandlers.end()) {
      ReportInternalError(wxString::Format(_T("Unrecognised command: \\%s"), command.c_str()), fullCommandString, t.offset);
      BumpErrors();
      return NeedMore;
    }
    else {
      return CALL_PSQL_HANDLER(*this, handler->second)(parameters, t);
    }
  }
}

void ScriptExecution::BeginQuery()
{
  queryBufferExecuted = true;
  bool added = owner->db->AddWorkOnlyIfConnected(new ScriptQueryWork(owner, queryBuffer));
  wxCHECK2(added, );
}

void ScriptExecution::BeginPutCopyData()
{
  bool added = owner->db->AddWorkOnlyIfConnected(new ScriptPutCopyDataWork(owner, CopyDataToken(), buffer.data()));
  wxCHECK2(added, );
}

void ScriptExecution::FinishExecution()
{
  wxCommandEvent event(PQWX_ScriptExecutionFinishing);
  owner->AddPendingEvent(event);
}

class PsqlArgumentsParser {
public:
  PsqlArgumentsParser(const wxString &str) : str(str), ptr(0), len(str.length()) {}
  bool HasMoreArguments()
  {
    SkipWhitespace();
    return HaveMore();
  }
  wxString GetNextArgument()
  {
    SkipWhitespace();
    if (Peek() == _T('\''))
      return GetQuotedString();
    else
      return GetPlainString();
  }
private:
  void SkipWhitespace()
  {
    while (HaveMore() && iswspace(str[ptr]))
      ++ptr;
  }
  wxString GetQuotedString()
  {
    wxString out;
    ++ptr;
    while (HaveMore()) {
      wxChar c = Take();
      if (c == _T('\'')) {
        if (!HaveMore()) break;
        if (Peek() == _T('\'')) {
          // escaped quote mark
          out += _T('\'');
          Take();
        }
        else {
          break;
        }
      }
      else if (c == _T('\\')) {
        if (!HaveMore()) break; // illegal, really
        char c = Take();
        if (c == _T('\\'))
          out += c;
        else if (c == _T('n'))
          out += _T('\n');
        else if (c == _T('r'))
          out += _T('\r');
        else
          out += c;
      }
      else {
        out += c;
      }
    }
    return out;
  }
  wxString GetPlainString()
  {
    unsigned pos = ptr;
    while (HaveMore()) {
      wxChar c = Take();
      if (iswspace(c))
        break;
    }
    return str.Mid(pos, ptr - pos);
  }
  bool HaveMore() const
  {
    return ptr < len;
  }
  wxChar Peek() const
  {
    return HaveMore() ? str[ptr] : ((wxChar) -1);
  }
  wxChar Take()
  {
    return HaveMore() ? str[ptr++] : ((wxChar) -1);
  }
  const wxString &str;
  unsigned ptr;
  unsigned len;
};

ScriptExecution::NextState ScriptExecution::PsqlChangeDatabase(const wxString &parameters, const ExecutionLexer::Token &t)
{
  // TODO handle connection problems...
  wxString newDbName = owner->db->DbName();
  owner->db->Dispose();
  delete owner->db;
  owner->db = NULL;
  owner->ShowDisconnectedStatus();
  owner->UpdateStateInUI(); // refresh title etc
  PsqlArgumentsParser tkz(parameters);
  if (tkz.HasMoreArguments()) {
    wxString t = tkz.GetNextArgument();
    if (t != _T("-")) newDbName = t;
  }
  if (tkz.HasMoreArguments()) {
    wxString t = tkz.GetNextArgument();
    if (t != _T("-")) owner->server.username = t;
  }
  if (tkz.HasMoreArguments()) {
    wxString t = tkz.GetNextArgument();
    if (t != _T("-")) owner->server.hostname = t;
  }
  if (tkz.HasMoreArguments()) {
    wxString t = tkz.GetNextArgument();
    if (t != _T("-")) {
      long port;
      if (t.ToLong(&port))
        owner->server.port = port;
      else {
        // syntax error...
        wxASSERT_MSG(false, wxString::Format(_T("port syntax error: %s"), t.c_str()));
      }
    }
  }
  if (tkz.HasMoreArguments()) {
    // syntax error...
    wxASSERT_MSG(false, wxString::Format(_T("too many parameters: %s"), parameters.c_str()));
  }
  owner->Connect(owner->server, newDbName);
  return NeedMore;
}

ScriptExecution::NextState ScriptExecution::PsqlExecuteQueryBuffer(const wxString &parameters, const ExecutionLexer::Token &t)
{
  BeginQuery();
  return NoMore;
}

ScriptExecution::NextState ScriptExecution::PsqlPrintQueryBuffer(const wxString &parameters, const ExecutionLexer::Token &t)
{
  owner->GetOrCreateResultsBook()->ScriptEcho(queryBuffer, t.offset);
  return NeedMore;
}

ScriptExecution::NextState ScriptExecution::PsqlResetQueryBuffer(const wxString &parameters, const ExecutionLexer::Token &t)
{
  queryBuffer.clear();
  return NeedMore;
}

ScriptExecution::NextState ScriptExecution::PsqlPrintMessage(const wxString &parameters, const ExecutionLexer::Token &t)
{
  PsqlArgumentsParser tkz(parameters);
  wxString output;
  while (tkz.HasMoreArguments()) {
    if (!output.empty()) output += _T(' ');
    output += tkz.GetNextArgument();
  }
  owner->GetOrCreateResultsBook()->ScriptEcho(output, t.offset);
  return NeedMore;
}

void ScriptExecution::ProcessQueryResult(ScriptQueryWork::Result *result)
{
  if (result->status == PGRES_TUPLES_OK) {
    wxLogDebug(_T("%s (%u tuples)"), result->statusTag.c_str(), result->data->Rows().size());
    owner->GetOrCreateResultsBook()->ScriptResultSet(result->statusTag, *result->data, lastSqlPosition);
    AddRows(result->data->Rows().size());
  }
  else if (result->status == PGRES_COMMAND_OK) {
    wxLogDebug(_T("%s (no tuples)"), result->statusTag.c_str());
    owner->GetOrCreateResultsBook()->ScriptCommandCompleted(result->statusTag, lastSqlPosition);
  }
  else if (result->status == PGRES_FATAL_ERROR) {
    wxLogDebug(_T("Got error: %s"), result->error.GetPrimary().c_str());
    owner->GetOrCreateResultsBook()->ScriptError(result->error, lastSqlPosition);
    BumpErrors();
  }

  owner->UpdateConnectionState(result->newConnectionState);
  owner->ShowTransactionStatus();

  delete result;
}

void ScriptExecution::ProcessConnectionNotice(const PgError& error)
{
  owner->GetOrCreateResultsBook()->ScriptQueryNotice(error, lastSqlPosition);
}

ScriptExecution::NextState ScriptExecution::PsqlQuitExecution(const wxString &parameters, const ExecutionLexer::Token &t)
{
  return Finish;
}
// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
