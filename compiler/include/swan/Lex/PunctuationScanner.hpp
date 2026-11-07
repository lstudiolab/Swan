#pragma once
#include "swan/Lex/Lexer.hpp"
#include <string>
namespace swan {
inline Token Lexer::scanPunctuation() {
    const auto line = line_;
    const auto column = column_;
    const char first = peek();
    const char second = peek(1);
    if ((first == '/' && second == '/') || (first == '-' && second == '>') || (first == '=' && second == '=') ||
        (first == '!' && second == '=') || (first == '<' && second == '=') ||
        (first == '>' && second == '=') || (first == '&' && second == '&') ||
        (first == '|' && second == '|')) {
        std::string_view kind;
        if (first == '/') kind = "//";
        else if (first == '-') kind = "->";
        else if (first == '=') kind = "==";
        else if (first == '!') kind = "!=";
        else if (first == '<') kind = "<=";
        else if (first == '>') kind = ">=";
        else if (first == '&') kind = "&&";
        else kind = "||";
        const char pair[] = {first, second};
        advance();
        advance();
        return {kind, std::string(pair, 2), line, column};
    }
    std::string_view kind;
    switch (first) {
    case '(': kind = "("; break;
    case ')': kind = ")"; break;
    case '[': kind = "["; break;
    case ']': kind = "]"; break;
    case ',': kind = ","; break;
    case ':': kind = ":"; break;
    case ';': kind = ";"; break;
    case '.': kind = "."; break;
    case '+': kind = "+"; break;
    case '-': kind = "-"; break;
    case '*': kind = "*"; break;
    case '/': kind = "/"; break;
    case '%': kind = "%"; break;
    case '=': kind = "="; break;
    case '<': kind = "<"; break;
    case '>': kind = ">"; break;
    case '!': kind = "!"; break;
    default:
        fail({"", "", line, column},
             std::string("unexpected character '") + first + "'");
    }
    advance();
    return {kind, std::string(1, first), line, column};
}
} // namespace swan
