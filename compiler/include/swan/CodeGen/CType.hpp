#pragma once
#include "swan/Basic/Diagnostic.hpp"
#include <string>
#include <unordered_set>
namespace swan {
inline std::string mapCType(const std::string& type,
                            const std::unordered_set<std::string>& structures) {
    if (type == "int") return "int";
    if (type == "float") return "double";
    if (type == "bool") return "bool";
    if (type == "string") return "const char *";
    if (type == "void") return "void";
    if (structures.contains(type)) return type;
    throw CompileError("unknown C type '" + type + "'");
}
}
