#pragma once
#include <string_view>
namespace swan {
inline bool isNumericType(std::string_view type) {
    return type == "int" || type == "float";
}
inline bool isTypeCompatible(std::string_view expected, std::string_view actual) {
    return expected == actual || (expected == "float" && actual == "int");
}
}
