#pragma once
#include "swan/Lex/Lexer.hpp"
#include <string>
#include <string_view>
#include <utility>

namespace swan {
inline Token Lexer::scanPunctuation() {
    const auto line = line_;
    const auto column = column_;
    const char character = peek();
    const char next = peek(1);

    if ((character == '-' && next == '>') ||
        (character == '=' && next == '=') ||
        (character == '!' && next == '=') ||
        (character == '<' && next == '=') ||
        (character == '>' && next == '=') ||
        (character == '&' && next == '&') ||
        (character == '|' && next == '|')) {
        advance();
        advance();
        std::string_view kind;
        if (character == '-') kind = "->";
        else if (character == '=') kind = "==";
        else if (character == '!') kind = "!=";
        else if (character == '<') kind = "<=";
        else if (character == '>') kind = ">=";
        else if (character == '&') kind = "&&";
        else kind = "||";
        return {kind, std::string{character, next}, line, column};
    }

    if (std::string_view("{}()[],:;.+-*/%=<>!").find(character) !=
        std::string_view::npos) {
        std::string_view kind;
        switch (character) {
            case '{': kind = "{"; break; case '}': kind = "}"; break;
            case '(': kind = "("; break; case ')': kind = ")"; break;
            case '[': kind = "["; break; case ']': kind = "]"; break;
            case ',': kind = ","; break; case ':': kind = ":"; break;
            case ';': kind = ";"; break; case '.': kind = "."; break;
            case '+': kind = "+"; break; case '-': kind = "-"; break;
            case '*': kind = "*"; break; case '/': kind = "/"; break;
            case '%': kind = "%"; break; case '=': kind = "="; break;
            case '<': kind = "<"; break; case '>': kind = ">"; break;
            case '!': kind = "!"; break;
        }
        advance();
        return {kind, std::string(1, character), line, column};
    }

    fail({"", "", line, column},
         std::string("unexpected character '") + character + "'");
}
}
