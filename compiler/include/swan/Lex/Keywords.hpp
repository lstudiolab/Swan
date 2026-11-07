#pragma once
#include <string_view>

namespace swan {
inline std::string_view keywordKind(std::string_view value) noexcept {
    if (value.empty()) return "IDENT";

    switch (value.front()) {
        case 'a':
            if (value == "and") return "and";
            if (value == "as") return "as";
            if (value == "async") return "async";
            if (value == "await") return "await";
            break;
        case 'b':
            if (value == "bool") return "bool";
            if (value == "break") return "break";
            break;
        case 'c':
            if (value == "case") return "case";
            if (value == "class") return "class";
            if (value == "clone") return "clone";
            if (value == "const") return "const";
            if (value == "continue") return "continue";
            break;
        case 'd':
            if (value == "deinit") return "deinit";
            break;
        case 'e':
            if (value == "else") return "else";
            if (value == "enum") return "enum";
            if (value == "export") return "export";
            break;
        case 'f':
            if (value == "false") return "false";
            if (value == "float") return "float";
            if (value == "fn") return "fn";
            if (value == "for") return "for";
            break;
        case 'g':
            if (value == "get") return "get";
            break;
        case 'h':
            if (value == "hold") return "hold";
            break;
        case 'i':
            if (value == "if") return "if";
            if (value == "import") return "import";
            if (value == "init") return "init";
            if (value == "int") return "int";
            break;
        case 'l':
            if (value == "let") return "let";
            if (value == "log") return "log";
            if (value == "loop") return "loop";
            break;
        case 'n':
            if (value == "none") return "none";
            if (value == "not") return "not";
            break;
        case 'o':
            if (value == "operate") return "operate";
            if (value == "or") return "or";
            break;
        case 'p':
            if (value == "pass") return "pass";
            if (value == "private") return "private";
            if (value == "public") return "public";
            break;
        case 'r':
            if (value == "range") return "range";
            if (value == "release") return "release";
            if (value == "return") return "return";
            break;
        case 's':
            if (value == "self") return "self";
            if (value == "store") return "store";
            if (value == "string") return "string";
            if (value == "struct") return "struct";
            if (value == "sync") return "sync";
            break;
        case 't':
            if (value == "true") return "true";
            if (value == "type") return "type";
            break;
        case 'v':
            if (value == "var") return "var";
            if (value == "void") return "void";
            break;
        case 'w':
            if (value == "while") return "while";
            break;
    }

    return "IDENT";
}

inline bool isKeyword(std::string_view value) noexcept {
    return keywordKind(value) != "IDENT";
}
} // namespace swan
