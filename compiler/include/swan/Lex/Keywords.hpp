#pragma once
#include <algorithm>
#include <array>
#include <string_view>
namespace swan {
inline bool isKeyword(std::string_view value) {
    static constexpr std::array keywords = {
        std::string_view{"fn"}, "let", "var", "const", "struct", "if", "else",
        "while", "return", "break", "continue", "pass", "log", "true", "false",
        "none", "int", "float", "bool", "string", "void", "and", "or", "not",
        "import", "as", "for", "loop", "async", "await", "sync", "hold",
        "store", "operate", "clone", "get", "init", "deinit", "release",
        "enum", "class", "type", "public", "private", "export", "self",
        "case", "range"
    };
    return std::find(keywords.begin(), keywords.end(), value) != keywords.end();
}
}
