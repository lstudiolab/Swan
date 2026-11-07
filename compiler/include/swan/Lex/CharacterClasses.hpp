#pragma once
namespace swan {
inline bool isIdentifierStart(char value) noexcept {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') || value == '_';
}
inline bool isIdentifierContinue(char value) noexcept {
    return isIdentifierStart(value) || isDecimalDigit(value);
}
inline bool isDecimalDigit(char value) noexcept {
    return value >= '0' && value <= '9';
}
inline bool isHorizontalWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r';
}
} // namespace swan
