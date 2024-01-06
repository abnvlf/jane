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
  TokenIdKeywordPub,
  TokenIdKeywordExport,
  TokenIdKeywordAs,
  TokenIdKeywordUse,
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
  TokenIdBoolOr,
  TokenIdBoolAnd,
  TokenIdBinOr,
  TokenIdBinAnd,
  TokenIdBinXor,
  TokenIdEq,
  TokenIdCmpEq,
  TokenIdBang,
  TokenIdTilde,
  TokenIdCmpNotEq,
  TokenIdCmpLessThan,
  TokenIdCmpGreaterThan,
  TokenIdCmpLessOrEq,
  TokenIdCmpGreaterOrEq,
  TokenIdBitShiftLeft,
  TokenIdBitShiftRight,
  TokenIdSlash,
  TokenIdPercent,
};

struct Token {
  TokenId id;
  int start_position;
  int end_position;
  int start_line;
  int start_column;
};

JaneList<Token> *tokenize(Buf *buf);
void print_tokens(Buf *buf, JaneList<Token> *tokens);

#endif // JANE_TOKENIZER