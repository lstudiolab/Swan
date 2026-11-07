#pragma once
#include <string_view>
namespace swan {
inline bool isKeyword(std::string_view value) noexcept {
    switch (value.size()) {
    case 2: return value == "fn" || value == "if" || value == "or" || value == "as";
    case 3: return value == "let" || value == "var" || value == "log" ||
                   value == "int" || value == "and" || value == "not" ||
                   value == "for" || value == "get";
    case 4: return value == "else" || value == "pass" || value == "true" ||
                   value == "none" || value == "bool" || value == "void" ||
                   value == "loop" || value == "sync" || value == "hold" ||
                   value == "enum" || value == "type" || value == "self" ||
                   value == "case" || value == "init";
    case 5: return value == "const" || value == "while" || value == "break" ||
                   value == "false" || value == "float" || value == "store" ||
                   value == "async" || value == "await" || value == "clone" ||
                   value == "class" || value == "range";
    case 6: return value == "struct" || value == "return" || value == "import" ||
                   value == "string" || value == "deinit" || value == "public" ||
                   value == "export";
    case 7: return value == "operate" || value == "private" || value == "release";
    case 8: return value == "continue";
    default: return false;
    }
}
}
