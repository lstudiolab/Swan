#pragma once
#include <string>
namespace swan {
inline bool isNumericType(const std::string& type) {
    return type == "int" || type == "float";
}
inline bool isTypeCompatible(const std::string& expected, const std::string& actual) {
    return expected == actual || (expected == "float" && actual == "int");
}
}
