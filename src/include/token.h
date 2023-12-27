#ifndef JANE_TOKEN
#define JANE_TOKEN

#include "list.h"
#include "util.h"
#include <assert.h>
#include <cstdio>
#include <stdarg.h>
#include <stdlib.h>

#define WHITESPACE                                                             \
  ' ' : case '\t':                                                             \
  case '\n':                                                                   \
  case '\f':                                                                   \
  case '\r':                                                                   \
  case 0xb

#define DIGIT                                                                  \
  '0' : case '1':                                                              \
  case '2':                                                                    \
  case '3':                                                                    \
  case '4':                                                                    \
  case '5':                                                                    \
  case '6':                                                                    \
  case '7':                                                                    \
  case '8':                                                                    \
  case '9'

#define ALPHA                                                                  \
  'a' : case 'b':                                                              \
  case 'c':                                                                    \
  case 'd':                                                                    \
  case 'e':                                                                    \
  case 'f':                                                                    \
  case 'g':                                                                    \
  case 'h':                                                                    \
  case 'i':                                                                    \
  case 'j':                                                                    \
  case 'k':                                                                    \
  case 'l':                                                                    \
  case 'm':                                                                    \
  case 'n':                                                                    \
  case 'o':                                                                    \
  case 'p':                                                                    \
  case 'q':                                                                    \
  case 'r':                                                                    \
  case 's':                                                                    \
  case 't':                                                                    \
  case 'u':                                                                    \
  case 'v':                                                                    \
  case 'w':                                                                    \
  case 'x':                                                                    \
  case 'y':                                                                    \
  case 'z':                                                                    \
  case 'A':                                                                    \
  case 'B':                                                                    \
  case 'C':                                                                    \
  case 'D':                                                                    \
  case 'E':                                                                    \
  case 'F':                                                                    \
  case 'G':                                                                    \
  case 'H':                                                                    \
  case 'I':                                                                    \
  case 'J':                                                                    \
  case 'K':                                                                    \
  case 'L':                                                                    \
  case 'M':                                                                    \
  case 'N':                                                                    \
  case 'O':                                                                    \
  case 'P':                                                                    \
  case 'Q':                                                                    \
  case 'R':                                                                    \
  case 'S':                                                                    \
  case 'T':                                                                    \
  case 'U':                                                                    \
  case 'V':                                                                    \
  case 'W':                                                                    \
  case 'X':                                                                    \
  case 'Y':                                                                    \
  case 'Z'

enum TokenId {
  TokenIdDirective,
  TokenIdSymbol,
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
};

struct Token {
  TokenId id;
  int start_pos;
  int end_pos;
};

enum TokenizeState {
  TokenizeStateStart,
  TokenizeStateDirective,
  TokenizeStateSymbol,
  TokenizeStateString,
  TokenizeStateNumber,
};

struct Tokenize {
  int pos;
  TokenizeState state;
  JaneList<Token> *tokens;
  int line;
  int column;
  Token *cur_tok;
};

static void tokenize_error(Tokenize *t, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "ERROR, line %d, column %d: ", t->line + 1, t->column + 1);
  vfprintf(stderr, format, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

static void begin_token(Tokenize *t, TokenId id) {
  assert(!t->cur_tok);
  t->tokens->add_one();
  Token *token = &t->tokens->last();
  token->id = id;
  token->start_pos = t->pos;
  t->cur_tok = token;
}

static void end_token(Tokenize *t) {
  assert(t->cur_tok);
  t->cur_tok->end_pos = t->pos + 1;
  t->cur_tok = nullptr;
}

static void put_back(Tokenize *t, int count) { t->pos -= count; }

static JaneList<Token> *tokenize(Buf *buf) {
  Tokenize t = {0};
  t.tokens = allocate<JaneList<Token>>(1);
  for (t.pos = 0; t.pos < buf->len; t.pos += 1) {
    uint8_t c = buf->ptr[t.pos];
    switch (t.state) {
    case TokenizeStateStart:
      switch (c) {
      case WHITESPACE:
        break;
      case ALPHA:
        t.state = TokenizeStateSymbol;
        begin_token(&t, TokenIdSymbol);
        break;
      case DIGIT:
        t.state = TokenizeStateNumber;
        begin_token(&t, TokenIdNumberLiteral);
        break;
      case '#':
        t.state = TokenizeStateDirective;
        begin_token(&t, TokenIdDirective);
        break;
      case '(':
        begin_token(&t, TokenIdLParen);
        end_token(&t);
        break;
      case ')':
        begin_token(&t, TokenIdLParen);
        end_token(&t);
        break;
      case ',':
        begin_token(&t, TokenIdComma);
        end_token(&t);
        break;
      case '*':
        begin_token(&t, TokenIdStar);
        end_token(&t);
        break;
      case '{':
        begin_token(&t, TokenIdLBrace);
        end_token(&t);
        break;
      case '}':
        begin_token(&t, TokenIdRBrace);
        end_token(&t);
        break;
      case '"':
        begin_token(&t, TokenIdStringLiteral);
        t.state = TokenizeStateString;
        break;
      case ';':
        begin_token(&t, TokenIdSemicolon);
        end_token(&t);
        break;
      case '+':
        begin_token(&t, TokenIdPlus);
        end_token(&t);
        break;
      default:
        tokenize_error(&t, "invalid character: `%c`", c);
      }
      break;
    case TokenizeStateDirective:
      if (c == '\n') {
        assert(t.cur_tok);
        t.cur_tok->end_pos = t.pos;
        t.cur_tok = nullptr;
        t.state = TokenizeStateStart;
      }
      break;
    case TokenizeStateSymbol:
      switch (c) {
      case ALPHA:
      case DIGIT:
      case '_':
        break;
      default:
        put_back(&t, 1);
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      }
      break;
    case TokenizeStateString:
      switch (c) {
      case '"':
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      default:
        break;
      }
      break;
    case TokenizeStateNumber:
      switch (c) {
      case DIGIT:
        break;
      default:
        put_back(&t, 1);
        end_token(&t);
        t.state = TokenizeStateStart;
        break;
      }
      break;
    }
    if (c == '\n') {
      t.line += 1;
      t.column = 0;
    } else {
      t.column += 1;
    }
  }
  return t.tokens;
}

static const char *token_name(Token *token) {
  switch (token->id) {
  case TokenIdDirective:
    return "Directive";
  case TokenIdSymbol:
    return "Symbol";
  case TokenIdLParen:
    return "LParen";
  case TokenIdRParen:
    return "RParen";
  case TokenIdComma:
    return "Comma";
  case TokenIdStar:
    return "Star";
  case TokenIdLBrace:
    return "LBrace";
  case TokenIdRBrace:
    return "RBrace";
  case TokenIdStringLiteral:
    return "StringLiteral";
  case TokenIdSemicolon:
    return "Semicolon";
  case TokenIdNumberLiteral:
    return "NumberLiteral";
  case TokenIdPlus:
    return "Plus";
  }
  return "(invalid token)";
}

static void print_token(Buf *buf, JaneList<Token> *tokens) {
  for (int i = 0; i < tokens->length; i += 1) {
    Token *token = &tokens->at(i);
    printf("%s ", token_name(token));
    fwrite(buf->ptr + token->start_pos, 1, token->end_pos - token->start_pos,
           stdout);
    printf("\n");
  }
}

#endif // JANE_TOKEN