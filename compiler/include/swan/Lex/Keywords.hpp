#pragma once
#include <algorithm>
#include <array>
#include <string_view>

namespace swan {
inline bool isKeyword(std::string_view value) {
    static constexpr std::array<std::string_view, 48> keywords = {
        "and", "as", "async", "await", "bool", "break", "case", "class",
        "clone", "const", "continue", "deinit", "else", "enum", "export",
        "false", "float", "fn", "for", "get", "hold", "if", "import", "init",
        "int", "let", "log", "loop", "none", "not", "operate", "or", "pass",
        "private", "public", "range", "release", "return", "self", "store",
        "string", "struct", "sync", "true", "type", "var", "void", "while"
    };
    return std::binary_search(keywords.begin(), keywords.end(), value);
}
}
