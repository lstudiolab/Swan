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
    do { ++index_; }
    while (index_ < source_.size() && isIdentifierContinue(source_[index_]));
    const std::string_view text = source_.substr(start, index_ - start);
    column_ += text.size();

    const auto kind = keywordKind(text);
    if (kind.empty())
        return {"IDENT", std::string(text), line, column};

    // Most keywords are represented entirely by their token kind. Keep values
    // only for operators and primitive type names consumed by the parser.
    if (kind == "and" || kind == "or" || kind == "not" ||
        kind == "int" || kind == "float" || kind == "bool" ||
        kind == "void" || kind == "string")
        return {kind, std::string(kind), line, column};

    return {kind, "", line, column};
}
} // namespace swan
