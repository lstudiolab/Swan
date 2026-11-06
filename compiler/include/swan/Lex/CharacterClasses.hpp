#pragma once
#include <cctype>
namespace swan {
inline bool isIdentifierStart(char value) noexcept {
    return std::isalpha(static_cast<unsigned char>(value)) != 0 || value == '_';
}
inline bool isIdentifierContinue(char value) noexcept {
    return std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '_';
}
inline bool isDecimalDigit(char value) noexcept {
    return std::isdigit(static_cast<unsigned char>(value)) != 0;
}
inline bool isHorizontalWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r';
}
} // namespace swan
