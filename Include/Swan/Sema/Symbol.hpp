#pragma once

#include "Swan/Sema/Type.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace swan {

enum class SymbolKind {
    Variable,
    Parameter,
    Function,
    Structure,
    Field,
    Builtin
};

struct Symbol {
    std::string name;
    SymbolKind kind = SymbolKind::Variable;
    Type type;
    bool mutableValue = false;
    std::size_t parameterCount = 0;
    std::vector<std::string> fields;

    Symbol() = default;
    Symbol(std::string name, SymbolKind kind, Type type, bool mutableValue = false)
        : name(std::move(name)), kind(kind), type(std::move(type)),
          mutableValue(mutableValue) {}
};

} // namespace swan
