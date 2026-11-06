#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <string>
#include <utility>

namespace swan {
inline std::string CGenerator::inferredType(const ExprPtr& node) const {
    if (!node) return {};
    switch (node->kind) {
    case Expr::Kind::Literal:
        if (node->literal_kind == "INT") return "int";
        if (node->literal_kind == "FLOAT") return "float";
        if (node->literal_kind == "STRING") return "string";
        if (node->literal_kind == "true" || node->literal_kind == "false") return "bool";
        return {};
    case Expr::Kind::Name: {
        auto found = symbols_.find(node->value);
        return found == symbols_.end() ? "" : found->second;
    }
    case Expr::Kind::Call:
        if (node->left->kind == Expr::Kind::Name) {
            auto found = functions_.find(node->left->value);
            if (found != functions_.end()) return found->second->return_type;
        }
        return {};
    case Expr::Kind::Unary: return inferredType(node->left);
    case Expr::Kind::Binary:
        if (node->op == "==" || node->op == "!=" || node->op == "<" ||
            node->op == ">" || node->op == "<=" || node->op == ">=" ||
            node->op == "and" || node->op == "or" || node->op == "&&" || node->op == "||")
            return "bool";
        if (node->op == "/") return "float";
        return inferredType(node->left) == "float" || inferredType(node->right) == "float"
            ? "float" : inferredType(node->left);
    case Expr::Kind::Member: return {};
    }
    return {};
}

inline std::string CGenerator::expression(const ExprPtr& node) const {
    switch (node->kind) {
    case Expr::Kind::Literal:
        if (node->literal_kind == "STRING") return quote(node->value);
        if (node->literal_kind == "true") return "true";
        if (node->literal_kind == "false") return "false";
        if (node->literal_kind == "none") return "NULL";
        return node->value;
    case Expr::Kind::Name: return node->value;
    case Expr::Kind::Unary: return (node->op == "not" ? "!" : node->op) + expression(node->left);
    case Expr::Kind::Binary: {
        const auto left = expression(node->left), right = expression(node->right);
        if (node->op == "/") return "((double)(" + left + ") / (double)(" + right + "))";
        const auto op = node->op == "and" ? "&&" : node->op == "or" ? "||" : node->op;
        return "(" + left + " " + op + " " + right + ")";
    }
    case Expr::Kind::Call: {
        std::string result = expression(node->left) + "(";
        for (std::size_t i = 0; i < node->arguments.size(); ++i) {
            if (i) result += ", ";
            result += expression(node->arguments[i]);
        }
        return result + ")";
    }
    case Expr::Kind::Member: return expression(node->left) + "." + node->value;
    }
    return {};
}
}
