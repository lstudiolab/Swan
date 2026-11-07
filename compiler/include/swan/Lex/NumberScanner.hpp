#pragma once
#include "swan/Lex/Lexer.hpp"
#include "swan/Lex/CharacterClasses.hpp"
#include <string>
namespace swan {
inline Token Lexer::scanNumber() {
    const auto line = line_;
    const auto column = column_;
    const auto start = index_;
    while (index_ < source_.size() && isDecimalDigit(source_[index_]))
        ++index_;

    bool floating = false;
    if (index_ + 1 < source_.size() && source_[index_] == '.' &&
        isDecimalDigit(source_[index_ + 1])) {
        floating = true;
        ++index_;
        while (index_ < source_.size() && isDecimalDigit(source_[index_]))
            ++index_;
    }

    // Update the source column once instead of once per digit.
    column_ += index_ - start;
    return {floating ? "FLOAT" : "INT",
            std::string(source_.substr(start, index_ - start)), line, column};
}
} // namespace swan
