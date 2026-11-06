#pragma once

#include "Swan/Frontend/Token.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace swan {

struct LexerError {
    SourceLocation location;
    std::string message;
};

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();
    const std::vector<LexerError>& errors() const noexcept;

private:
    std::string source_;
    std::size_t offset_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;

    std::vector<Token> tokens_;
    std::vector<LexerError> errors_;
    std::vector<std::size_t> indentation_{0};

    bool atEnd() const noexcept;
    char current(std::size_t lookahead = 0) const noexcept;
    char advance() noexcept;
    bool match(char expected) noexcept;
    SourceLocation location() const noexcept;

    void scanToken();
    void scanIdentifier();
    void scanNumber();
    void scanString(const std::string& opening, const std::string& closing);
    void scanLine();
    void emit(TokenKind kind, std::size_t start, SourceLocation where);
    void error(SourceLocation where, std::string message);
    void applyIndentation(std::size_t width, SourceLocation where);
    void emitPendingDedents(SourceLocation where);

    static TokenKind keywordKind(std::string_view text) noexcept;
};

} // namespace swan
