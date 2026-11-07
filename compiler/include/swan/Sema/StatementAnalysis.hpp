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
    scopes_.back().reserve(variableCount);
    for (const auto& statement : statements) analyzeStatement(statement);
    scopes_.pop_back();
}

inline void Sema::analyzeStatement(const StmtPtr& node) {
    switch (node->kind) {
    case Stmt::Kind::Variable: {
        if (scopes_.back().contains(node->name))
            fail(node->token,"'" + node->name + "' is already declared in this scope");
        std::string actual = node->value ? analyzeExpression(node->value) : "";
        std::string type = node->type.empty() ? actual : node->type;
        if (type.empty()) fail(node->token,"cannot infer type for '" + node->name + "'");
        if (!types_.contains(type)) fail(node->token,"unknown type '" + type + "'");
        if (type == "void") fail(node->token,"variables cannot have type void");
        if (!actual.empty() && !compatible(type,actual))
            fail(node->token,"cannot initialize " + type + " with " + actual);
        scopes_.back()[node->name] = {type,node->mutable_value};
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
            fail(node->token,"function returns " + return_type_ + ", but this return has type " + actual);
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
