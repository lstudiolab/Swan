#pragma once
#include "swan/Sema/Sema.hpp"
#include <string>
#include <utility>

namespace swan {
inline Sema::Sema(const Program& program) : program_(program) {}

inline bool Sema::numeric(std::string_view type) { return isNumericType(type); }

inline bool Sema::compatible(std::string_view expected, std::string_view actual) {
    return isTypeCompatible(expected, actual);
}

inline Sema::Symbol* Sema::lookup(std::string_view name) {
    const auto found = active_symbols_.find(name);
    return found == active_symbols_.end() ? nullptr : found->second;
}
}
