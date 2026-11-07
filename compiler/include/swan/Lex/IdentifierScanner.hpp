#pragma once
#include "swan/Lex/Lexer.hpp"
#include "swan/Lex/CharacterClasses.hpp"
#include "swan/Lex/Keywords.hpp"
#include <string>
#include <utility>
namespace swan {
inline Token Lexer::scanIdentifier() {
    const auto line = line_;
    const auto column = column_;
    const auto start = index_;
    while (index_ < source_.size() && isIdentifierContinue(source_[index_])) {
        ++index_;
        ++column_;
    }
    std::string value(source_.substr(start, index_ - start));
    std::string kind = isKeyword(value) ? value : "IDENT";
    return {std::move(kind), std::move(value), line, column};
}
} // namespace swan
