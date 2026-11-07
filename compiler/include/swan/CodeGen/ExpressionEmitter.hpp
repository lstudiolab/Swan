#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <string>
#include <string_view>
#include <utility>

namespace swan {
inline std::string CGenerator::inferredType(const ExprPtr& node) const {
    return node ? node->resolved_type : std::string{};
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
