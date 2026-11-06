#pragma once

#include <string>
#include <utility>

namespace swan {

enum class TypeKind {
    Unknown,
    Int,
    Float,
    String,
    Bool,
    None,
    Array,
    Function,
    Struct
};

struct Type {
    TypeKind kind = TypeKind::Unknown;
    std::string name;

    Type() = default;
    explicit Type(TypeKind kind) : kind(kind) {}
    Type(TypeKind kind, std::string name)
        : kind(kind), name(std::move(name)) {}

    bool isNumeric() const noexcept {
        return kind == TypeKind::Int || kind == TypeKind::Float;
    }
    bool isUnknown() const noexcept { return kind == TypeKind::Unknown; }
    bool operator==(const Type& other) const noexcept {
        return kind == other.kind && name == other.name;
    }
    bool operator!=(const Type& other) const noexcept { return !(*this == other); }

    static Type unknown() { return Type(TypeKind::Unknown); }
    static Type integer() { return Type(TypeKind::Int); }
    static Type floating() { return Type(TypeKind::Float); }
    static Type string() { return Type(TypeKind::String); }
    static Type boolean() { return Type(TypeKind::Bool); }
    static Type none() { return Type(TypeKind::None); }
    static Type array(Type element) {
        return Type(TypeKind::Array, element.toString());
    }
    static Type function() { return Type(TypeKind::Function); }
    static Type structure(std::string name) {
        return Type(TypeKind::Struct, std::move(name));
    }

    std::string toString() const {
        switch (kind) {
            case TypeKind::Int: return "int";
            case TypeKind::Float: return "float";
            case TypeKind::String: return "string";
            case TypeKind::Bool: return "bool";
            case TypeKind::None: return "none";
            case TypeKind::Array: return "[" + name + "]";
            case TypeKind::Function: return "function";
            case TypeKind::Struct: return name;
            case TypeKind::Unknown: default: return "unknown";
        }
    }
};

} // namespace swan
