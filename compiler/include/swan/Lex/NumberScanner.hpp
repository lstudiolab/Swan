#pragma once
#include "swan/Lex/Lexer.hpp"
#include "swan/Lex/CharacterClasses.hpp"
#include <string>
namespace swan {
inline Token Lexer::scanNumber() {
    const auto line = line_;
    const auto column = column_;
    std::string value;
    while (!eof() && isDecimalDigit(peek()))
        value += advance();
    std::string kind = "INT";
    if (!eof() && peek() == '.' && isDecimalDigit(peek(1))) {
        kind = "FLOAT";
        value += advance();
        while (!eof() && isDecimalDigit(peek()))
            value += advance();
    }
    return {kind, value, line, column};
}
} // namespace swan
