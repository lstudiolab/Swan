#pragma once
#include <string_view>
namespace swan {
inline bool isKeyword(std::string_view value) noexcept {
    switch (value.size()) {
    case 2:
        return value == "fn" || value == "if" || value == "or" ||
               value == "as";
    case 3:
        return value == "let" || value == "var" || value == "for" ||
               value == "log" || value == "int" || value == "and" ||
               value == "not" || value == "get" || value == "new";
    case 4:
        return value == "else" || value == "true" || value == "none" ||
               value == "bool" || value == "void" || value == "loop" ||
               value == "hold" || value == "sync" || value == "enum" ||
               value == "type" || value == "self" || value == "case" ||
               value == "init" || value == "async" || value == "await";
    case 5:
        return value == "const" || value == "while" || value == "break" ||
               value == "float" || value == "store" || value == "clone" ||
               value == "deinit" || value == "class" || value == "range";
    case 6:
        return value == "struct" || value == "return" || value == "import" ||
               value == "string" || value == "public" || value == "export";
    case 7:
        return value == "continue" || value == "private" || value == "release" ||
               value == "operate";
    case 8:
        return value == "function";
    case 5 + 4:
        return value == "statement";
    default:
        return false;
    }
}
}
