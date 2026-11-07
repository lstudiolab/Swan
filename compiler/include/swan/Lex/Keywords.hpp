#pragma once
#include <algorithm>
#include <array>
#include <string_view>

namespace swan {
inline constexpr std::array<std::string_view, 48> keywordTable = {
    "and", "as", "async", "await", "bool", "break", "case", "class",
    "clone", "const", "continue", "deinit", "else", "enum", "export",
    "false", "float", "fn", "for", "get", "hold", "if", "import", "init",
    "int", "let", "log", "loop", "none", "not", "operate", "or", "pass", "private",
    "public", "range", "release", "return", "self", "store", "string", "struct",
    "sync", "true", "type", "var", "void", "while"
};
inline std::string_view keywordKind(std::string_view value) {
    const auto found = std::lower_bound(keywordTable.begin(), keywordTable.end(), value);
    return found != keywordTable.end() && *found == value ? *found : "IDENT";
}
inline bool isKeyword(std::string_view value) {
    return keywordKind(value) != "IDENT";
}
}
