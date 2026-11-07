#pragma once
#include "swan/Lex/Lexer.hpp"
#include <string>
#include <string_view>
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
        return {value, value, line, column};
    }
    if (std::string_view("()[],:;.+-*/%=<>!").find(first) != std::string_view::npos) {
        advance();
        const std::string value(1, first);
        return {value, value, line, column};
    }
    fail({"", "", line, column},
         std::string("unexpected character '") + first + "'");
}
} // namespace swan
