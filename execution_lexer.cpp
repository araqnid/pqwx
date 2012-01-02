#include "execution_lexer.h"

ExecutionLexer::Token ExecutionLexer::Pull0()
{
  SkipWhitespace();
  if (Done()) return Token(Token::END);

  int start = pos;
  Token::Type type;
  if (Peek() == '\\') {
    type = Token::PSQL;
    ++pos;
  }
  else {
    type = Token::SQL;
  }

  do {
    int c = Take();

    if (c < 0) {
      return Token(type, start, pos - start);
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
	return Token(type, start, pos - start + 1);
      }
      else if (c == '\\') {
	// exclusive end character
	BackUp();
	return Token(type, start, pos - start);
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
