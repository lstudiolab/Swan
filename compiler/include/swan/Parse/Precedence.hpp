#pragma once
#include <string_view>
namespace swan {
inline int operatorPrecedence(std::string_view op) {
    if (op == "=") return 1;
    if (op == "or" || op == "||") return 2;
    if (op == "and" || op == "&&") return 3;
    if (op == "==" || op == "!=") return 4;
    if (op == "<" || op == "<=" || op == ">" || op == ">=") return 5;
    if (op == "+" || op == "-") return 6;
    if (op == "*" || op == "/" || op == "%") return 7;
    return 0;
}
}
