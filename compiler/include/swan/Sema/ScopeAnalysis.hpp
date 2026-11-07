#pragma once
#include "swan/Sema/Sema.hpp"
#include <string>
#include <string_view>
#include <utility>
namespace swan {
inline Sema::Sema(const Program& program) : program_(program) {}
inline bool Sema::numeric(const std::string& type) { return isNumericType(type); }
inline bool Sema::compatible(const std::string& expected,const std::string& actual) {
    return isTypeCompatible(expected, actual);
}
inline std::pair<std::string,bool>* Sema::lookup(std::string_view name) {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto found = scope->find(name);
        if (found != scope->end()) return &found->second;
    }
    return nullptr;
}
}
