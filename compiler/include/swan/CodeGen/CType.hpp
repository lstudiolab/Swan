#pragma once
#include "swan/Basic/Diagnostic.hpp"
#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace swan {
inline std::string cTypeName(const std::string& name) { return "swan_type_" + name; }
inline std::string cVariableName(const std::string& name) { return "swan_var_" + name; }
inline std::string cFieldName(const std::string& name) { return "swan_field_" + name; }
inline std::string cFunctionName(const std::string& name) { return name == "main" ? "main" : "swan_fn_" + name; }

struct TransparentStringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
};
template <typename Value>
using StringViewMap = std::unordered_map<std::string_view, Value, TransparentStringHash, std::equal_to<>>;
using StructureSet = std::unordered_set<std::string_view, TransparentStringHash, std::equal_to<>>;

inline void appendMappedCType(std::string& output, std::string_view type,
                              const StructureSet& structures) {
    if (type == "int") output += "int";
    else if (type == "float") output += "double";
    else if (type == "bool") output += "bool";
    else if (type == "string") output += "const char *";
    else if (type == "void") output += "void";
    else if (structures.contains(type)) {
        output += "swan_type_";
        output.append(type);
    } else {
        throw CompileError("unknown C type '" + std::string(type) + "'");
    }
}

inline std::string mapCType(const std::string& type, const StructureSet& structures) {
    std::string result;
    result.reserve(type.size() + 8);
    appendMappedCType(result, type, structures);
    return result;
}
}
