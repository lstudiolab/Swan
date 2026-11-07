#pragma once
#include "swan/Lex/Lexer.hpp"
#include <string>
namespace swan {
inline Token Lexer::scanPunctuation() {
    const auto line = line_;
    const auto column = column_;
    const char first = peek();
    const char second = peek(1);
    if ((first == '-' && second == '>') || (first == '=' && second == '=') ||
        (first == '!' && second == '=') || (first == '<' && second == '=') ||
        (first == '>' && second == '=') || (first == '&' && second == '&') ||
        (first == '|' && second == '|')) {
        const char pair[] = {first, second};
        advance();
        advance();
        const std::string value(pair, 2);
        return {std::string_view(source_.data() + index_ - 2, 2), value, line, column};
    }
    switch (first) {
    case '(': case ')': case '[': case ']': case ',': case ':': case ';':
    case '.': case '+': case '-': case '*': case '/': case '%': case '=':
    case '<': case '>': case '!': {
        advance();
        const std::string value(1, first);
        return {std::string_view(source_.data() + index_ - 1, 1), value, line, column};
    }
    default:
        fail({"", "", line, column},
             std::string("unexpected character '") + first + "'");
    }
}
} // namespace swan
