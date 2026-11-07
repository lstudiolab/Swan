#pragma once
#include "swan/Lex/Lexer.hpp"
#include "swan/Lex/CharacterClasses.hpp"
#include <string>
namespace swan {
inline Token Lexer::scanNumber() {
    const auto line = line_;
    const auto column = column_;
    const auto start = index_;

    do {
        ++index_;
    } while (index_ < source_.size() &&
             isDecimalDigit(source_[index_]));

    bool floating = false;
    if (index_ + 1 < source_.size() && source_[index_] == '.' &&
        isDecimalDigit(source_[index_ + 1])) {
        floating = true;
        index_ += 2;
        while (index_ < source_.size() &&
               isDecimalDigit(source_[index_]))
            ++index_;
    }

    const auto length = index_ - start;
    column_ += length;
    return {floating ? "FLOAT" : "INT",
            std::string(source_.substr(start, length)), line, column};
}
} // namespace swan
