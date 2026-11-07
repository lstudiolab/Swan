#pragma once
#include "swan/Lex/Lexer.hpp"
#include "swan/Lex/CharacterClasses.hpp"
#include "swan/Lex/Keywords.hpp"
#include <string>
namespace swan {
inline Token Lexer::scanIdentifier() {
    const auto line = line_;
    const auto column = column_;
    const auto start = index_;
    do { ++index_; ++column_; }
    while (index_ < source_.size() && isIdentifierContinue(source_[index_]));
    const std::string_view text = source_.substr(start, index_ - start);
    const std::string value(text);
    return {isKeyword(text) ? value : "IDENT", value, line, column - (index_ - start)};
}
} // namespace swan
