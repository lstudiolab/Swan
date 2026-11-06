#pragma once

#include "Swan/Sema/Symbol.hpp"

#include <string>
#include <unordered_map>

namespace swan {

class Scope {
public:
    explicit Scope(Scope* parent = nullptr) noexcept : parent_(parent) {}

    Scope* parent() const noexcept { return parent_; }

    bool define(Symbol symbol) {
        const auto name = symbol.name;
        return symbols_.emplace(name, std::move(symbol)).second;
    }

    Symbol* lookupLocal(const std::string& name) noexcept {
        const auto found = symbols_.find(name);
        return found == symbols_.end() ? nullptr : &found->second;
    }

    const Symbol* lookupLocal(const std::string& name) const noexcept {
        const auto found = symbols_.find(name);
        return found == symbols_.end() ? nullptr : &found->second;
    }

    Symbol* lookup(const std::string& name) noexcept {
        for (auto* scope = this; scope; scope = scope->parent_) {
            if (auto* symbol = scope->lookupLocal(name)) return symbol;
        }
        return nullptr;
    }

    const Symbol* lookup(const std::string& name) const noexcept {
        for (auto* scope = this; scope; scope = scope->parent_) {
            if (const auto* symbol = scope->lookupLocal(name)) return symbol;
        }
        return nullptr;
    }

private:
    Scope* parent_ = nullptr;
    std::unordered_map<std::string, Symbol> symbols_;
};

} // namespace swan
