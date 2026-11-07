#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <string>
#include <string_view>
#include <utility>

namespace swan {
inline std::string_view CGenerator::inferredType(const ExprPtr& node) const {
    return node ? std::string_view(node->resolved_type) : std::string_view{};
}

inline std::string CGenerator::expression(const ExprPtr& node) const {
    std::string result;
    result.reserve(64);
    appendExpression(node, result);
    return result;
}

inline void CGenerator::appendExpression(const ExprPtr& node, std::string& result) const {
    const auto& value = node->token.value;
    switch (node->kind) {
    case Expr::Kind::Literal:
        if (node->token.kind == "STRING") appendCString(result, value);
        else if (node->token.kind == "true") result += "true";
        else if (node->token.kind == "false") result += "false";
        else if (node->token.kind == "none") result += "NULL";
        else result.append(value);
        return;
    case Expr::Kind::Name:
        result += "swan_var_";
        result.append(value);
        return;
    case Expr::Kind::Unary:
        result += value == "not" ? "!" : value;
        appendExpression(node->left, result);
        return;
    case Expr::Kind::Binary:
        result.push_back('(');
        if (value == "/") result += "(double)(";
        appendExpression(node->left, result);
        if (value == "/") {
            result += ") / (double)(";
            appendExpression(node->right, result);
            result += "))";
            return;
        }
        result.push_back(' ');
        if (value == "and") result += "&&";
        else if (value == "or") result += "||";
        else result.append(value);
        result.push_back(' ');
        appendExpression(node->right, result);
        result.push_back(')');
        return;
    case Expr::Kind::Call:
        if (!node->left || node->left->kind != Expr::Kind::Name)
            fail(node->token, "only direct function calls are supported");
        if (node->left->token.value == "main") result += "main";
        else {
            result += "swan_fn_";
            result.append(node->left->token.value);
        }
        result.push_back('(');
        for (std::size_t i = 0; i < node->arguments.size(); ++i) {
            if (i) result += ", ";
            appendExpression(node->arguments[i], result);
        }
        result.push_back(')');
        return;
    case Expr::Kind::Member:
        appendExpression(node->left, result);
        result += ".swan_field_";
        result.append(value);
        return;
    }
}
}
