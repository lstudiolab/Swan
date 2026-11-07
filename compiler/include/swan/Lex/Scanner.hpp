#pragma once
#include "swan/Lex/Lexer.hpp"
#include "swan/Lex/CharacterClasses.hpp"
#include "swan/Lex/IdentifierScanner.hpp"
#include "swan/Lex/NumberScanner.hpp"
#include "swan/Lex/StringScanner.hpp"
#include "swan/Lex/PunctuationScanner.hpp"
#include <algorithm>

namespace swan {
inline Lexer::Lexer(std::string_view source) : source_(source) {}

inline bool Lexer::eof() const {
    return index_ >= source_.size();
}

inline char Lexer::peek(std::size_t offset) const {
    return offset < source_.size() - index_ ? source_[index_ + offset] : '\0';
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
    result.reserve(std::min<std::size_t>(source_.size() / 6 + 1, 65536));

    while (index_ < source_.size()) {
        const char character = source_[index_];

        if (isHorizontalWhitespace(character)) {
            do {
                ++index_;
                ++column_;
            } while (index_ < source_.size() &&
                     isHorizontalWhitespace(source_[index_]));
            continue;
        }

        if (character == '#') {
            const auto newline = source_.find('\n', index_);
            const auto end = newline == std::string_view::npos
                ? source_.size() : newline;
            column_ += end - index_;
            index_ = end;
            continue;
        }

        if (character == '\n') {
            result.push_back({"NEWLINE", "\\n", line_, column_});
            ++index_;
            ++line_;
            column_ = 1;
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
