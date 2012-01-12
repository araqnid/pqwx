#include "execution_lexer.h"

ExecutionLexer::Token ExecutionLexer::Pull0()
{
  SkipWhitespace();
  if (Done()) return Token(Token::END);

  if (Peek() == '\\') {
    return PullPsql();
  }
  else {
    return PullSql();
  }
}

ExecutionLexer::Token ExecutionLexer::PullPsql()
{
  int start = pos;
  Take(); // consume the starting backslash

  do {
    int c = Take();

    if (c < 0 || c == '\n') {
      return Token(Token::PSQL, start, pos - start);
    }
    else if (c == '\\') {
      if (Peek() == '\\') {
	// skip over terminating double-backslash
	Take();
	return Token(Token::PSQL, start, pos - start - 2);
      }
      else {
	// preserve starting backslash of next command
	BackUp();
	return Token(Token::PSQL, start, pos - start);
      }
    }
    else if (c == '\'') {
      PassSingleQuotedString(false);
    }

  } while (true);
}

ExecutionLexer::Token ExecutionLexer::PullSql()
{
  int start = pos;

  do {
    int c = Take();

    if (c < 0) {
      return Token(Token::SQL, start, pos - start);
    }

    // note that dollar-quoting takes priority over *everything* (except end of input of course)

    if (c == '$')
      ProcessDollarQuote();
    else if (quoteStack.empty()) {
      if (c == '\'')
	PassSingleQuotedString(false); // FIXME handle E'...' flag for escape syntax
      else if (c == '\"')
      PassDoubleQuotedString();
      else if (c == ';') {
	// inclusive end character
	int end = ++pos;
	SkipWhitespace();
	return Token(Token::SQL, start, end - start);
      }
      else if (c == '\\') {
	// exclusive end character
	BackUp();
	return Token(Token::SQL, start, pos - start);
      }
    }
  } while (true);
}

void ExecutionLexer::PassSingleQuotedString(bool escapeSyntax)
{
  do {
    int c = Take();
    if (c == '\'') {
      if (Peek() != '\'')
	return;
    }
  } while (true);
}

void ExecutionLexer::PassDoubleQuotedString()
{
  do {
    int c = Take();
    if (c == '\"') {
      if (Peek() != '\"')
	return;
    }
  } while (true);
}

void ExecutionLexer::ProcessDollarQuote()
{
  if (isdigit(Peek())) return; // $1 etc. are not dollar-quoted strings

  int start = pos;

  int c;
  do {
    c = Take();
    if (c < 0) return; // EOF inside dollar marker, oh well.
  } while (c != '$');

  std::string marker(buffer, pos - start);

  if (!quoteStack.empty() && quoteStack.back() == marker) {
    quoteStack.pop_back();
  }
  else {
    quoteStack.push_back(marker);
  }
}

void ExecutionLexer::SkipWhitespace()
{
  do {
    int c = Take();
    if (c < 0)
      return;
    if (!isspace((char) c)) {
      BackUp();
      return;
    }
  } while (true);
}

ExecutionLexer::Token ExecutionLexer::ReadCopyData0()
{
  int start = pos;
  bool backslashed = false;

  do {
    int c = Take();

    if (c < 0) {
      // well, this is really an error
      return Token(Token::COPY_DATA, start, pos - start);
    }

    if (backslashed) {
      if (c == '.') {
	// remove the "\." from the returned token
	return Token(Token::COPY_DATA, start, pos - 2 - start);
      }
      backslashed = false;
    }
    else {
      if (c == '\\')
	backslashed = true;
    }
  } while (true);
}
