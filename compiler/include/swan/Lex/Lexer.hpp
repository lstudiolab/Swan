#pragma once
#include "swan/Basic/Diagnostic.hpp"
#include "swan/Basic/Token.hpp"
#include "swan/Lex/Keywords.hpp"
#include <cstddef>
#include <string_view>
#include <vector>
namespace swan {
class Lexer {
public:
    explicit Lexer(std::string_view source);
    std::vector<Token> scan();
private:
    bool eof() const;
    char peek(std::size_t offset = 0) const;
    char advance();
    Token scanIdentifier();
    Token scanNumber();
    Token scanString();
    Token scanPunctuation();
    std::string_view source_;
    std::size_t index_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};
} // namespace swan
#include "swan/Lex/Scanner.hpp"
