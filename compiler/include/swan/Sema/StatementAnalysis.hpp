#pragma once
#include "swan/Sema/Sema.hpp"
#include <string>
#include <utility>

namespace swan {
inline bool Sema::alwaysReturns(const std::vector<StmtPtr>& statements) const {
    for (const auto& statement : statements) {
        if (statement->kind == Stmt::Kind::Return) return true;
        if (statement->kind == Stmt::Kind::If &&
            !statement->alternate.empty() &&
            alwaysReturns(statement->body) &&
            alwaysReturns(statement->alternate)) return true;
    }
    return false;
}

inline void Sema::analyzeBlock(const std::vector<StmtPtr>& statements) {
    scopes_.emplace_back();
    std::size_t variableCount = 0;
    for (const auto& statement : statements)
        variableCount += statement->kind == Stmt::Kind::Variable;
    scopes_.back().declarations.reserve(variableCount);
    scopes_.back().shadowed.reserve(variableCount);
    for (const auto& statement : statements) analyzeStatement(statement);
    for (auto previous = scopes_.back().shadowed.rbegin();
         previous != scopes_.back().shadowed.rend(); ++previous) {
        if (previous->second) active_symbols_.insert_or_assign(previous->first, previous->second);
        else active_symbols_.erase(previous->first);
    }
    scopes_.pop_back();
}

inline void Sema::analyzeStatement(const StmtPtr& node) {
    switch (node->kind) {
    case Stmt::Kind::Variable: {
        auto& scope = scopes_.back();
        if (scope.declarations.contains(node->name))
            fail(node->token,"'" + node->name + "' is already declared in this scope");
        const std::string_view actual = node->value ? analyzeExpression(node->value) : std::string_view{};
        std::string type = node->type.empty() ? std::string(actual) : node->type;
        if (type.empty()) fail(node->token,"cannot infer type for '" + node->name + "'");
        if (!types_.contains(type)) fail(node->token,"unknown type '" + type + "'");
        if (type == "void") fail(node->token,"variables cannot have type void");
        if (!actual.empty() && !compatible(type,actual))
            fail(node->token,"cannot initialize " + type + " with " + std::string(actual));
        const auto previous = active_symbols_.find(node->name);
        Symbol* shadowed = previous == active_symbols_.end() ? nullptr : previous->second;
        auto [symbol, inserted] = scope.declarations.emplace(
            node->name, Symbol{std::move(type), node->mutable_value});
        scope.shadowed.emplace_back(node->name, shadowed);
        active_symbols_.insert_or_assign(node->name, &symbol->second);
        break;
    }
    case Stmt::Kind::Log: {
        const auto type = analyzeExpression(node->value);
        if (type != "string" && type != "bool" && !numeric(type))
            fail(node->token,"log expects a string, number, or boolean");
        break;
    }
    case Stmt::Kind::Return: {
        const auto actual = node->value ? analyzeExpression(node->value) : "void";
        if (!compatible(return_type_,actual))
            fail(node->token,"function returns " + return_type_ + ", but this return has type " + std::string(actual));
        break;
    }
    case Stmt::Kind::Expression: analyzeExpression(node->value); break;
    case Stmt::Kind::If:
        if (analyzeExpression(node->condition) != "bool") fail(node->token,"if condition must be bool");
        analyzeBlock(node->body);
        if (!node->alternate.empty()) analyzeBlock(node->alternate);
        break;
    case Stmt::Kind::While:
        if (analyzeExpression(node->condition) != "bool") fail(node->token,"while condition must be bool");
        ++loop_depth_; analyzeBlock(node->body); --loop_depth_;
        break;
    case Stmt::Kind::Break:
    case Stmt::Kind::Continue:
        if (loop_depth_ == 0) fail(node->token,"loop control used outside a loop");
        break;
    case Stmt::Kind::Pass: break;
    default: fail(node->token,"statement is not supported in this context");
    }
}
}
