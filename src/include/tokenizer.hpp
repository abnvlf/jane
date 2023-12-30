#ifndef JANE_TOKENIZER
#define JANE_TOKENIZER

#include "buffer.hpp"

enum TokenId {
  TokenIdEof,
  TokenIdSymbol,
  TokenIdKeywordFn,
  TokenIdKeywordReturn,
  TokenIdKeywordMut,
  TokenIdKeywordConst,
  TokenIdKeywordExtern,
  TokenIdKeywordUnreachable,
  TokenIdLParen,
  TokenIdRParen,
  TokenIdComma,
  TokenIdStar,
  TokenIdLBrace,
  TokenIdRBrace,
  TokenIdStringLiteral,
  TokenIdSemicolon,
  TokenIdNumberLiteral,
  TokenIdPlus,
  TokenIdColon,
  TokenIdArrow,
  TokenIdDash,
  TokenIdNumberSign,
};

struct Token {
  TokenId id;
  int start_position;
  int end_position;
  int start_line;
  int start_column;
};

enum TokenizeState {
  TokenizeStateStart,
  TokenizeStateSymbol,
  TokenizeStateNumber,
  TokenizeStateString,
  TokenizeStateSawDash,
};

JaneList<Token> *tokenize(Buf *buf);
void print_tokens(Buf *buf, JaneList<Token> *tokens);

#endif // JANE_TOKENIZER