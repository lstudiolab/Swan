#pragma once
#include "swan/Lex/Lexer.hpp"
#include <string>
namespace swan {
inline Token Lexer::scanString() {
    const auto line = line_;
    const auto column = column_;
    const char quote = advance();
    std::string value;
    value.reserve(32);
    while (!eof() && peek() != quote) {
        if (peek() != '\\') {
            const auto start = index_;
            while (!eof() && peek() != quote && peek() != '\\') advance();
            value.append(source_.substr(start, index_ - start));
            continue;
        }
        advance();
        if (eof())
            fail({"STRING", value, line, column}, "unterminated escape sequence");
        switch (advance()) {
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            case '\\': value += '\\'; break;
            case '"': value += '"'; break;
            case '\'': value += '\''; break;
            default:
                fail({"STRING", value, line, column}, "unknown escape sequence");
        }
    }
    if (eof())
        fail({"STRING", value, line, column}, "unterminated string");
    advance();
    return {"STRING", std::move(value), line, column};
}
} // namespace swan
