#pragma once
#include "swan/Lex/Lexer.hpp"
#include <string>

namespace swan {
inline Token Lexer::scanPunctuation() {
    const auto line = line_;
    const auto column = column_;
    const char character = source_[index_];
    const char next = peek(1);

    switch (character) {
        case '-':
            if (next == '>') {
                index_ += 2;
                column_ += 2;
                return {"->", "->", line, column};
            }
            break;
        case '=':
            if (next == '=') {
                index_ += 2;
                column_ += 2;
                return {"==", "==", line, column};
            }
            break;
        case '!':
            if (next == '=') {
                index_ += 2;
                column_ += 2;
                return {"!=", "!=", line, column};
            }
            break;
        case '<':
            if (next == '=') {
                index_ += 2;
                column_ += 2;
                return {"<=", "<=", line, column};
            }
            break;
        case '>':
            if (next == '=') {
                index_ += 2;
                column_ += 2;
                return {">=", ">=", line, column};
            }
            break;
        case '&':
            if (next == '&') {
                index_ += 2;
                column_ += 2;
                return {"&&", "&&", line, column};
            }
            break;
        case '|':
            if (next == '|') {
                index_ += 2;
                column_ += 2;
                return {"||", "||", line, column};
            }
            break;
    }

    std::string_view kind;
    switch (character) {
        case '{': kind = "{"; break;
        case '}': kind = "}"; break;
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
                 std::string("unexpected character '") + character + "'");
    }

    ++index_;
    ++column_;
    return {kind, std::string(1, character), line, column};
}
} // namespace swan
