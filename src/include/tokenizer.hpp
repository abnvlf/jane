#ifndef JANE_TOKENIZER
#define JANE_TOKENIZER

#include "buffer.hpp"

enum TokenId {
  TokenIdStar = 0,
  TokenIdLParen = 1,
  TokenIdEof = 2,
  TokenIdSymbol,
  TokenIdKeywordFn,
  TokenIdKeywordReturn,
  TokenIdKeywordMut,
  TokenIdKeywordConst,
  TokenIdRParen,
  TokenIdComma,
  TokenIdLBrace,
  TokenIdRBrace,
  TokenIdStringLiteral,
  TokenIdSemicolon,
  TokenIdNumberLiteral,
  TokenIdPlus,
  TokenIdColon,
  TokenIdArrow,
  TokenIdDash,
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

JaneList<Token> *tokenize(Buf *buf, Buf *cur_dir_path);
void print_tokens(Buf *buf, JaneList<Token> *tokens);

#endif // JANE_TOKENIZER