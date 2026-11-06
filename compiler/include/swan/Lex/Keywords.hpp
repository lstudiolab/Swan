#pragma once
#include <string_view>
namespace swan {
inline bool isKeyword(std::string_view value) {
    static constexpr std::string_view keywords[] = {
        "fn", "let", "var", "const", "struct", "if", "else", "while", "return",
        "break", "continue", "pass", "log", "true", "false", "none", "int",
        "float", "bool", "string", "void", "and", "or", "not", "import", "as",
        "for", "loop", "async", "await", "sync", "hold", "store", "operate",
        "clone", "get", "init", "deinit", "release", "enum", "class", "type",
        "public", "private", "export", "self", "case", "range"
    };
    for (const auto keyword : keywords) if (keyword == value) return true;
    return false;
}
}
