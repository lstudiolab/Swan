#pragma once
#include <string_view>
namespace swan {
inline std::string_view keywordKind(std::string_view value) noexcept {
    switch (value.size()) {
    case 2:
        if (value == "fn") return "fn";
        if (value == "if") return "if";
        if (value == "or") return "or";
        if (value == "as") return "as";
        break;
    case 3:
        if (value == "let") return "let";
        if (value == "var") return "var";
        if (value == "log") return "log";
        if (value == "int") return "int";
        if (value == "and") return "and";
        if (value == "not") return "not";
        if (value == "for") return "for";
        if (value == "get") return "get";
        break;
    case 4:
        if (value == "else") return "else";
        if (value == "pass") return "pass";
        if (value == "true") return "true";
        if (value == "none") return "none";
        if (value == "bool") return "bool";
        if (value == "void") return "void";
        if (value == "loop") return "loop";
        if (value == "sync") return "sync";
        if (value == "hold") return "hold";
        if (value == "enum") return "enum";
        if (value == "type") return "type";
        if (value == "self") return "self";
        if (value == "case") return "case";
        if (value == "init") return "init";
        break;
    case 5:
        if (value == "const") return "const";
        if (value == "while") return "while";
        if (value == "break") return "break";
        if (value == "false") return "false";
        if (value == "float") return "float";
        if (value == "store") return "store";
        if (value == "async") return "async";
        if (value == "await") return "await";
        if (value == "clone") return "clone";
        if (value == "class") return "class";
        if (value == "range") return "range";
        break;
    case 6:
        if (value == "struct") return "struct";
        if (value == "return") return "return";
        if (value == "import") return "import";
        if (value == "string") return "string";
        if (value == "deinit") return "deinit";
        if (value == "public") return "public";
        if (value == "export") return "export";
        break;
    case 7:
        if (value == "operate") return "operate";
        if (value == "private") return "private";
        if (value == "release") return "release";
        break;
    case 8:
        if (value == "continue") return "continue";
        break;
    }
    return {};
}
inline bool isKeyword(std::string_view value) noexcept {
    return !keywordKind(value).empty();
}
}
