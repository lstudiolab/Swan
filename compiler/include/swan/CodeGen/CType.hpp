#pragma once
#include "swan/Basic/Diagnostic.hpp"
#include <string>
#include <string_view>
#include <unordered_set>
namespace swan {
inline std::string cTypeName(const std::string& name) { return "swan_type_" + name; }
inline std::string cVariableName(const std::string& name) { return "swan_var_" + name; }
inline std::string cFieldName(const std::string& name) { return "swan_field_" + name; }
inline std::string cFunctionName(const std::string& name) { return name == "main" ? "main" : "swan_fn_" + name; }
inline std::string mapCType(const std::string& type, const std::unordered_set<std::string_view>& structures) {
    if (type == "int") return "int64_t";
    if (type == "float") return "double";
    if (type == "bool") return "bool";
    if (type == "string") return "const char *";
    if (type == "void") return "void";
    if (structures.contains(type)) return cTypeName(type);
    throw CompileError("unknown C type '" + type + "'");
}
}
