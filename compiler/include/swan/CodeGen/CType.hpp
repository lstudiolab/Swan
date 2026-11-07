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

inline void appendMappedCType(std::string& output, std::string_view type,
                              const std::unordered_set<std::string>& structures) {
    if (type == "int") output += "int";
    else if (type == "float") output += "double";
    else if (type == "bool") output += "bool";
    else if (type == "string") output += "const char *";
    else if (type == "void") output += "void";
    else if (structures.contains(std::string(type))) {
        output += "swan_type_";
        output.append(type);
    } else {
        throw CompileError("unknown C type '" + std::string(type) + "'");
    }
}

inline std::string mapCType(const std::string& type, const std::unordered_set<std::string>& structures) {
    std::string result;
    result.reserve(type.size() + 8);
    appendMappedCType(result, type, structures);
    return result;
}
}
