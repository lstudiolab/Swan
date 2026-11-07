#pragma once
#include "swan/Lex/Lexer.hpp"
#include "swan/Lex/CharacterClasses.hpp"
#include "swan/Lex/IdentifierScanner.hpp"
#include "swan/Lex/NumberScanner.hpp"
#include "swan/Lex/StringScanner.hpp"
#include "swan/Lex/PunctuationScanner.hpp"
namespace swan {
inline Lexer::Lexer(std::string_view source) : source_(source) {}
inline bool Lexer::eof() const { return index_ >= source_.size(); }
inline char Lexer::peek(std::size_t offset) const {
    const auto position = index_ + offset;
    return position < source_.size() ? source_[position] : '\0';
}
inline char Lexer::advance() {
    const char value = source_[index_++];
    if (value == '\n') { ++line_; column_ = 1; }
    else ++column_;
    return value;
}
inline std::vector<Token> Lexer::scan() {
    std::vector<Token> result;
    result.reserve(source_.size() / 4 + 8);
    while (!eof()) {
        if (line_start_) {
            std::size_t width = 0;
            while (peek() == ' ' || peek() == '\t') {
                width += peek() == '\t' ? 4 : 1;
                advance();
            }
            if (peek() == '#' || peek() == '\n' || eof()) {
                while (!eof() && peek() != '\n') advance();
                if (!eof()) advance();
                continue;
            }
            line_start_ = false;
            if (width > indentation_.back()) {
                indentation_.push_back(width);
                result.push_back({"INDENT","",line_,column_});
            } else if (width < indentation_.back()) {
                while (indentation_.size() > 1 && width < indentation_.back()) {
                    indentation_.pop_back();
                    result.push_back({"DEDENT","",line_,column_});
                }
                if (width != indentation_.back())
                    fail({"","",line_,column_},"inconsistent indentation");
            }
        }
        const char character = peek();
        if (isHorizontalWhitespace(character)) { advance(); continue; }
        if (character == '#') {
            while (!eof() && peek() != '\n') advance();
            continue;
        }
        if (character == '\n') {
            result.push_back({"NEWLINE","\\n",line_,column_});
            advance();
            line_start_ = true;
            continue;
        }
        if (isIdentifierStart(character)) { result.push_back(scanIdentifier()); continue; }
        if (isDecimalDigit(character)) { result.push_back(scanNumber()); continue; }
        if (character == '"' || character == '\'') { result.push_back(scanString()); continue; }
        result.push_back(scanPunctuation());
    }
    while (indentation_.size() > 1) {
        indentation_.pop_back();
        result.push_back({"DEDENT","",line_,column_});
    }
    result.push_back({"EOF","",line_,column_});
    return result;
}
} // namespace swan
