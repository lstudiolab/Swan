#pragma once
#include "swan/Sema/Sema.hpp"
#include <string>
#include <utility>

namespace swan {
inline std::string Sema::analyzeExpression(const ExprPtr& node) {
    switch (node->kind) {
    case Expr::Kind::Literal:
        if (node->literal_kind == "INT") return "int";
        if (node->literal_kind == "FLOAT") return "float";
        if (node->literal_kind == "STRING") return "string";
        if (node->literal_kind == "true" || node->literal_kind == "false") return "bool";
        return "none";
    case Expr::Kind::Name: {
        auto* symbol = lookup(node->value);
        if (!symbol) fail(node->token,"unknown name '" + node->value + "'");
        return symbol->first;
    }
    case Expr::Kind::Unary: {
        const auto type = analyzeExpression(node->left);
        if (node->op == "!" || node->op == "not") {
            if (type != "bool") fail(node->token,"logical not requires bool");
            return "bool";
        }
        if (!numeric(type)) fail(node->token,"numeric negation requires a number");
        return type;
    }
    case Expr::Kind::Binary: {
        const auto left = analyzeExpression(node->left);
        if (node->op == "=") {
            if (node->left->kind != Expr::Kind::Name) fail(node->token,"assignment target must be a variable");
            auto* symbol = lookup(node->left->value);
            if (!symbol) fail(node->left->token,"unknown variable '" + node->left->value + "'");
            if (!symbol->second) fail(node->left->token,"cannot assign to immutable value '" + node->left->value + "'");
            const auto right = analyzeExpression(node->right);
            if (!compatible(symbol->first,right)) fail(node->token,"cannot assign " + right + " to " + symbol->first);
            return symbol->first;
        }
        const auto right = analyzeExpression(node->right);
        if (node->op == "and" || node->op == "or" || node->op == "&&" || node->op == "||") {
            if (left != "bool" || right != "bool") fail(node->token,"logical operators require bool operands");
            return "bool";
        }
        if (node->op == "==" || node->op == "!=" || node->op == "<" ||
            node->op == ">" || node->op == "<=" || node->op == ">=") {
            if (left != right && !(numeric(left) && numeric(right)))
                fail(node->token,"comparison operands have incompatible types");
            return "bool";
        }
        if (!numeric(left) || !numeric(right)) fail(node->token,"arithmetic operators require numeric operands");
        return (left == "float" || right == "float" || node->op == "/") ? "float" : "int";
    }
    case Expr::Kind::Call: {
        if (node->left->kind != Expr::Kind::Name) fail(node->token,"only direct function calls are supported");
        auto found = functions_.find(node->left->value);
        if (found == functions_.end()) fail(node->token,"unknown function '" + node->left->value + "'");
        const auto& parameters = found->second->parameters;
        if (parameters.size() != node->arguments.size())
            fail(node->token,"function '" + node->left->value + "' expects " +
                 std::to_string(parameters.size()) + " arguments, got " + std::to_string(node->arguments.size()));
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            const auto actual = analyzeExpression(node->arguments[i]);
            if (!compatible(parameters[i].type,actual))
                fail(node->arguments[i]->token,"argument has type " + actual + "; expected " + parameters[i].type);
        }
        return found->second->return_type;
    }
    case Expr::Kind::Member: fail(node->token,"member access is not implemented yet");
    }
    fail(node->token,"unsupported expression");
}
}
