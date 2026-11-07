#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <string>
#include <string_view>
#include <utility>

namespace swan {
inline std::string_view CGenerator::inferredType(const ExprPtr& node) const {
    if (!node) return {};
    switch (node->resolved_type) {
    case Expr::ResolvedType::Int: return "int";
    case Expr::ResolvedType::Float: return "float";
    case Expr::ResolvedType::Bool: return "bool";
    case Expr::ResolvedType::String: return "string";
    case Expr::ResolvedType::None: return "none";
    case Expr::ResolvedType::Void: return "void";
    case Expr::ResolvedType::Unknown: break;
    }
    switch (node->kind) {
    case Expr::Kind::Literal:
        if (node->literal_kind == "INT") return "int";
        if (node->literal_kind == "FLOAT") return "float";
        if (node->literal_kind == "STRING") return "string";
        if (node->literal_kind == "true" || node->literal_kind == "false") return "bool";
        return "none";
    case Expr::Kind::Name: {
        const auto found = symbols_.find(node->value);
        return found == symbols_.end() ? std::string_view{} : std::string_view(found->second);
    }
    case Expr::Kind::Call:
        if (node->left->kind == Expr::Kind::Name) {
            const auto found = functions_.find(node->left->value);
            if (found != functions_.end()) return found->second->return_type;
        }
        return {};
    case Expr::Kind::Unary: return inferredType(node->left);
    case Expr::Kind::Binary: {
        if (node->op == "==" || node->op == "!=" || node->op == "<" ||
            node->op == ">" || node->op == "<=" || node->op == ">=" ||
            node->op == "and" || node->op == "or" || node->op == "&&" || node->op == "||")
            return "bool";
        if (node->op == "/") return "float";
        const auto leftType = inferredType(node->left);
        const auto rightType = inferredType(node->right);
        return leftType == "float" || rightType == "float" ? "float" : leftType;
    }
    case Expr::Kind::Member: return {};
    }
    return {};
}
inline std::string CGenerator::expression(const ExprPtr& node) const {
    std::string result;
    result.reserve(32);
    appendExpression(result, node);
    return result;
}

inline void CGenerator::appendExpression(std::string& output, const ExprPtr& node) const {
    switch (node->kind) {
    case Expr::Kind::Literal:
        if (node->literal_kind == "STRING") appendEscapedCString(output, node->value);
        else if (node->literal_kind == "true") output += "true";
        else if (node->literal_kind == "false") output += "false";
        else if (node->literal_kind == "none") output += "NULL";
        else output += node->value;
        return;
    case Expr::Kind::Name:
        output += "swan_var_";
        output += node->value;
        return;
    case Expr::Kind::Unary:
        output += node->op == "not" ? "!" : node->op;
        appendExpression(output, node->left);
        return;
    case Expr::Kind::Binary:
        if (node->op == "/") {
            output += "((double)(";
            appendExpression(output, node->left);
            output += ") / (double)(";
            appendExpression(output, node->right);
            output += "))";
            return;
        }
        output.push_back('(');
        appendExpression(output, node->left);
        output.push_back(' ');
        if (node->op == "and") output += "&&";
        else if (node->op == "or") output += "||";
        else output += node->op;
        output.push_back(' ');
        appendExpression(output, node->right);
        output.push_back(')');
        return;
    case Expr::Kind::Call:
        if (node->left->kind != Expr::Kind::Name)
            fail(node->token,"only direct function calls are supported");
        if (node->left->value == "main") output += "main";
        else {
            output += "swan_fn_";
            output += node->left->value;
        }
        output.push_back('(');
        for (std::size_t i = 0; i < node->arguments.size(); ++i) {
            if (i) output += ", ";
            appendExpression(output, node->arguments[i]);
        }
        output.push_back(')');
        return;
    case Expr::Kind::Member:
        appendExpression(output, node->left);
        output.push_back('.');
        output += "swan_field_";
        output += node->value;
        return;
    }
}
}
