#pragma once
#include <cstddef>
#include <string>
namespace swan {
enum class TokenKind {
  End, Identifier, Integer, Float, String,
  LParen, RParen, LBrace, RBrace, Comma, Colon, Semicolon,
  Plus, Minus, Star, Slash, Percent, Equal, EqualEqual, Bang, BangEqual,
  Less, LessEqual, Greater, GreaterEqual, Arrow,
  KwFunc, KwStruct, KwFalse, KwTrue, KwIf, KwElse, KwWhile, KwAs,
  KwFrom, KwLoop, KwNull, KwReturn, KwGuard, KwIn, KwBlock, KwBreak,
  KwContinue, KwConst, KwVar, KwLet, KwExtern, KwIntern, KwImport,
  KwEnum, KwType, KwInt, KwInit, KwDeinit, KwBool,
  KwI8, KwI16, KwI32, KwI64, KwU8, KwU16, KwU32, KwU64,
  KwVarint, KwF32, KwF64, Unknown
};
struct SourceLocation { std::size_t offset=0, line=1, column=1; };
struct Token {
  TokenKind kind=TokenKind::Unknown;
  std::string lexeme;
  SourceLocation location;
};
const char* tokenName(TokenKind kind);
}
