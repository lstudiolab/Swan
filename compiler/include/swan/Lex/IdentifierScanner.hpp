#pragma once
#include "swan/Lex/Lexer.hpp"
#include "swan/Lex/CharacterClasses.hpp"
#include "swan/Lex/Keywords.hpp"
#include <string>
namespace swan {
inline Token Lexer::scanIdentifier() {
    const auto line = line_;
    const auto column = column_;
    std::string value;
    while (!eof() && isIdentifierContinue(peek()))
        value += advance();
    return {isKeyword(value) ? value : "IDENT", value, line, column};
}
} // namespace swan
