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

inline std::pair<std::string,bool>* Sema::lookup(const std::string& name) {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) return &found->second;
    }
    return nullptr;
}
}
