#pragma once
#include "swan/Lex/Lexer.hpp"
#include <string>
#include <string_view>
namespace swan {
inline Token Lexer::scanPunctuation() {
    const auto line = line_;
    const auto column = column_;
    const char character = peek();
    const std::string pair{character, peek(1)};
    if (pair == "->" || pair == "==" || pair == "!=" || pair == "<=" ||
        pair == ">=" || pair == "&&" || pair == "||") {
        advance(); advance();
        return {pair, pair, line, column};
    }
    if (std::string_view("()[],:;.+-*/%=<>!").find(character) != std::string_view::npos) {
        advance();
        const std::string value(1, character);
        return {value, value, line, column};
    }
    fail({"", "", line, column},
         std::string("unexpected character '") + character + "'");
}
} // namespace swan
