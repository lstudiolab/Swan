#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <string>
#include <string_view>
#include <utility>

namespace swan {
inline std::string CGenerator::inferredType(const ExprPtr& node) const {
    if (!node) return {};
    switch (node->kind) {
    case Expr::Kind::Literal:
        if (node->token.kind == "INT") return "int";
        if (node->token.kind == "FLOAT") return "float";
        if (node->token.kind == "STRING") return "string";
        if (node->token.kind == "true" || node->token.kind == "false") return "bool";
        return {};
    case Expr::Kind::Name:
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            const auto found = scope->find(node->token.value);
            if (found != scope->end()) return found->second;
        }
        return {};
    case Expr::Kind::Call:
        if (node->left->kind == Expr::Kind::Name) {
            auto found = functions_.find(node->left->token.value);
            if (found != functions_.end()) return found->second->return_type;
        }
        return {};
    case Expr::Kind::Unary: return inferredType(node->left);
    case Expr::Kind::Binary: {
        const auto& op = node->token.value;
        if (op == "==" || op == "!=" || op == "<" || op == ">" ||
            op == "<=" || op == ">=" || op == "and" || op == "or" ||
            op == "&&" || op == "||") return "bool";
        if (op == "/") return "float";
        const auto left = inferredType(node->left);
        if (left == "float") return "float";
        const auto right = inferredType(node->right);
        return right == "float" ? "float" : left;
    }
    case Expr::Kind::Member: return {};
    }
    return {};
}
inline std::string CGenerator::expression(const ExprPtr& node) const {
    const auto& value = node->token.value;
    switch (node->kind) {
    case Expr::Kind::Literal:
        if (node->token.kind == "STRING") return quote(value);
        if (node->token.kind == "true") return "true";
        if (node->token.kind == "false") return "false";
        if (node->token.kind == "none") return "NULL";
        return value;
    case Expr::Kind::Name: return cVariableName(value);
    case Expr::Kind::Unary: return (value == "not" ? "!" : value) + expression(node->left);
    case Expr::Kind::Binary: {
        const auto left = expression(node->left);
        const auto right = expression(node->right);
        if (value == "/") return "((double)(" + left + ") / (double)(" + right + "))";
        const std::string_view op = value == "and" ? "&&" :
                                    value == "or" ? "||" : value;
        return "(" + left + " " + std::string(op) + " " + right + ")";
    }
    case Expr::Kind::Call: {
        if (node->left->kind != Expr::Kind::Name) fail(node->token,"only direct function calls are supported");
        std::string result = cFunctionName(node->left->token.value) + "(";
        result.reserve(result.size() + node->arguments.size() * 16);
        for (std::size_t i = 0; i < node->arguments.size(); ++i) {
            if (i) result += ", ";
            result += expression(node->arguments[i]);
        }
        return result + ")";
    }
    case Expr::Kind::Member: return expression(node->left) + "." + cFieldName(value);
    }
    return {};
}
}
