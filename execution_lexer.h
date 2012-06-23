/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __execution_lexer_h
#define __execution_lexer_h

#include <string>
#include <vector>
#include "wx/string.h"
#include "wx/log.h"

/**
 * Lex through SQL text, breaking it into SQL commands and backslash commands.
 *
 * This should basically do what psql's lexer does, although I haven't
 * simply coped that wholesale.  This is based on UTF-8 rather than
 * wxChar, which matches how the data is stored in Scintilla, hence
 * the use of std::string.
 *
 * A lexer is created on a character buffer, and then Pull() called
 * repeatedly, until it returns a token with type
 * ExecutionLexer::Token::END.
 */
class ExecutionLexer {
public:
  /**
   * Create a lexer from a string buffer.
   */
  ExecutionLexer(const char *buffer, unsigned length) : buffer(buffer), length(length), pos(0) {
    wxASSERT(buffer != NULL);
  }

  /**
   * A unit returned by the lexer.
   */
  class Token {
  public:
    /**
     * Lexer token type.
     */
    enum Type {
      /**
       * A SQL statement.
       */
      SQL,
      /**
       * A psql directive.
       */
      PSQL,
      /**
       * Data for COPY command.
       */
      COPY_DATA,
      /**
       * End of input.
       */
      END
    } type;
    /**
     * Offset and length of the token within the lexer's input buffer.
     */
    unsigned offset, length;
    /**
     * Create token with given type and extent.
     */
    Token(Type type, unsigned offset, unsigned length) : type(type), offset(offset), length(length) {}
    /**
     * Create token with given type and invalid extent (for END).
     */
    Token(Type type) : type(type), offset((unsigned)-1), length((unsigned)-1) { wxASSERT(type == END); }
  };

  /**
   * Pull the next token from the buffer.
   */
  Token Pull()
  {
    if (pos >= length) return Token(Token::END);
    return Pull0();
  }
  /**
   * Read copy data from the buffer.
   */
  Token ReadCopyData()
  {
    if (pos >= length) return Token(Token::END);
    return ReadCopyData0();
  }

  /**
   * Convert an offset/length combination to a wxString.
   */
  wxString GetWXString(unsigned offset, unsigned length) const { return wxString(buffer + offset, wxConvUTF8, length); }
  /**
   * Convert a token into a wxString.
   */
  wxString GetWXString(const Token& t) const { return GetWXString(t.offset, t.length); }

private:
  std::vector<std::string> quoteStack;
  const char *buffer;
  const unsigned length;
  unsigned pos;
  void PassWhitespace();
  char CharAt(unsigned ofs) const { return buffer[ofs]; }
  int Peek() const { return pos >= length ? -1 : CharAt(pos); }
  int Take() { return pos >= length ? -1 : CharAt(pos++); }
  void BackUp() { --pos; }
  bool Done() const { return pos >= length; }
  void PassDoubleQuotedString();
  void PassSingleQuotedString(bool escapeSyntax);
  void PassSingleLineComment();
  void PassBlockComment();
  void PassDollarQuote();
  Token Pull0();
  Token PullPsql();
  Token PullSql();
  Token ReadCopyData0();
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
