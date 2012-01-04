// -*- mode: c++ -*-

#ifndef __execution_lexer_h
#define __execution_lexer_h

#include <string>
#include <vector>
#include "wx/string.h"

// Lex through SQL text, breaking it into SQL commands and backslash commands.
// This should basically do what psql's lexer does, although I haven't simply coped that wholesale.
// This is based on UTF-8 rather than wxChar, which matches how the
// data is stored in Scintilla, hence the use of std::string.
class ExecutionLexer {
public:
  ExecutionLexer(const char *buffer, unsigned length) : buffer(buffer), length(length), pos(0) {}

  class Token {
  public:
    const enum Type { SQL, PSQL, END } type;
    const unsigned offset, length;
    Token(Type type, unsigned offset, unsigned length) : type(type), offset(offset), length(length) {}
    Token(Type type) : type(type), offset((unsigned)-1), length((unsigned)-1) {}
  };

  Token Pull() { if (pos >= length) return Token(Token::END); return Pull0(); }

  wxString GetWXString(unsigned offset, unsigned length) const { return wxString(buffer + offset, wxConvUTF8, length); }
  wxString GetWXString(const Token& t) const { return GetWXString(t.offset, t.length); }
private:
  std::vector<std::string> quoteStack;
  const char *buffer;
  const unsigned length;
  unsigned pos;
  void SkipWhitespace();
  char CharAt(unsigned ofs) const { return buffer[ofs]; }
  int Peek() const { return pos >= length ? -1 : CharAt(pos); }
  int Take() { return pos >= length ? -1 : CharAt(pos++); }
  void BackUp() { --pos; }
  bool Done() const { return pos >= length; }
  void PassDoubleQuotedString();
  void PassSingleQuotedString(bool escapeSyntax);
  void ProcessDollarQuote();
  Token Pull0();
  Token PullPsql();
  Token PullSql();
};

#endif
