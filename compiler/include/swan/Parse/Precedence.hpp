#pragma once
#include <string_view>
namespace swan {
inline int operatorPrecedence(std::string_view op) noexcept {
    switch (op.size()) {
    case 1:
        switch (op.front()) {
        case '=': return 1;
        case '<':
        case '>': return 5;
        case '+':
        case '-': return 6;
        case '*':
        case '/':
        case '%': return 7;
        default: return 0;
        }
    case 2:
        if (op == "or" || op == "||") return 2;
        if (op == "&&") return 3;
        if (op == "==" || op == "!=") return 4;
        if (op == "<=" || op == ">=") return 5;
        return 0;
    case 3:
        return op == "and" ? 3 : 0;
    default:
        return 0;
    }
}
}
