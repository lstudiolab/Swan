#pragma once
#include "swan/Lex/Lexer.hpp"
#include "swan/Lex/CharacterClasses.hpp"
#include "swan/Lex/IdentifierScanner.hpp"
#include "swan/Lex/NumberScanner.hpp"
#include "swan/Lex/StringScanner.hpp"
#include "swan/Lex/PunctuationScanner.hpp"
namespace swan {
inline Lexer::Lexer(std::string_view source) : source_(source) {}
inline bool Lexer::eof() const {
    return index_ >= source_.size();
}
inline char Lexer::peek(std::size_t offset) const {
    return index_ + offset < source_.size() ? source_[index_ + offset] : '\0';
}
inline char Lexer::advance() {
    const char value = source_[index_++];
    if (value == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return value;
}
inline std::vector<Token> Lexer::scan() {
    std::vector<Token> result;
    while (!eof()) {
        const char character = peek();
        if (isHorizontalWhitespace(character)) {
            advance();
            continue;
        }
        if (character == '#') {
            while (!eof() && peek() != '\n')
                advance();
            continue;
        }
        if (character == '\n') {
            result.push_back({"NEWLINE", "\\n", line_, column_});
            advance();
            continue;
        }
        if (isIdentifierStart(character)) {
            result.push_back(scanIdentifier());
            continue;
        }
        if (isDecimalDigit(character)) {
            result.push_back(scanNumber());
            continue;
        }
        if (character == '"' || character == '\'') {
            result.push_back(scanString());
            continue;
        }
        result.push_back(scanPunctuation());
    }
    result.push_back({"EOF", "", line_, column_});
    return result;
}
} // namespace swan
