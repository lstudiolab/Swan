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

    do {
        ++index_;
    } while (index_ < source_.size() &&
             isIdentifierContinue(source_[index_]));

    const auto length = index_ - start;
    column_ += length;
    const auto text = source_.substr(start, length);
    return {keywordKind(text), std::string(text), line, column};
}
} // namespace swan
