#pragma once
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

inline std::string_view keywordKind(std::string_view value) noexcept {
    if (value.empty()) return "IDENT";

    switch (value.front()) {
        case 'a':
            if (value == "and" || value == "as" || value == "async" ||
                value == "await") return value;
            break;
        case 'b':
            if (value == "bool" || value == "break") return value;
            break;
        case 'c':
            if (value == "case" || value == "class" || value == "clone" ||
                value == "const" || value == "continue") return value;
            break;
        case 'd':
            if (value == "deinit") return value;
            break;
        case 'e':
            if (value == "else" || value == "enum" || value == "export")
                return value;
            break;
        case 'f':
            if (value == "false" || value == "float" || value == "fn" ||
                value == "for") return value;
            break;
        case 'g':
            if (value == "get") return value;
            break;
        case 'h':
            if (value == "hold") return value;
            break;
        case 'i':
            if (value == "if" || value == "import" || value == "init" ||
                value == "int") return value;
            break;
        case 'l':
            if (value == "let" || value == "log" || value == "loop")
                return value;
            break;
        case 'n':
            if (value == "none" || value == "not") return value;
            break;
        case 'o':
            if (value == "operate" || value == "or") return value;
            break;
        case 'p':
            if (value == "pass" || value == "private" || value == "public")
                return value;
            break;
        case 'r':
            if (value == "range" || value == "release" || value == "return")
                return value;
            break;
        case 's':
            if (value == "self" || value == "store" || value == "string" ||
                value == "struct" || value == "sync") return value;
            break;
        case 't':
            if (value == "true" || value == "type") return value;
            break;
        case 'v':
            if (value == "var" || value == "void") return value;
            break;
        case 'w':
            if (value == "while") return value;
            break;
    }

    return "IDENT";
}

inline bool isKeyword(std::string_view value) {
    return keywordKind(value) != "IDENT";
}
} // namespace swan
