#pragma once

#include <cstddef>
#include <string>
#include <variant>

namespace swan {

enum class TokenKind {
    EndOfFile,
    Newline,
    Indent,
    Dedent,

    Identifier,
    Integer,
    Float,
    String,

    // Declarations and types
    Let, Var, Const, Fn, Struct, Type, Enum, Class, Int,
    Public, Private, Import, Export, As, Self,

    // Control flow
    If, Else, EndIf, While, For, Loop, Range, Case,
    Break, Continue, Return, Pass,

    // Execution
    Async, Await, Sync, Operate,

    // Memory and ownership
    Hold, Store, Clone, Get, Init, Deinit, Release,

    // Built-in values and operations
    True, False, None, Log,

    // Punctuation
    Colon, Semicolon, Comma, Dot,
    LeftParen, RightParen,
    LeftBracket, RightBracket,
    LeftBrace, RightBrace,

    // Operators
    Assign, Plus, Minus, Star, Slash, Percent,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    And, Or, Not,
    Arrow,

    Unknown
};

struct SourceLocation {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

struct Token {
    TokenKind kind = TokenKind::Unknown;
    std::string lexeme;
    SourceLocation location;
};

const char* tokenKindName(TokenKind kind) noexcept;

} // namespace swan
