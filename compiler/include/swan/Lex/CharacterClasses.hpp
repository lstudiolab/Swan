#pragma once

namespace swan {
inline bool isIdentifierStart(char value) noexcept {
    const auto c = static_cast<unsigned char>(value);
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

inline bool isIdentifierContinue(char value) noexcept {
    const auto c = static_cast<unsigned char>(value);
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

inline bool isDecimalDigit(char value) noexcept {
    const auto c = static_cast<unsigned char>(value);
    return c >= '0' && c <= '9';
}

inline bool isHorizontalWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r';
}
} // namespace swan
