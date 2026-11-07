#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <string>
#include <utility>

namespace swan {
inline void CGenerator::line(const std::string& value) {
    output_.append(indent_ * 4, ' ');
    output_.append(value);
    output_.push_back('\n');
}
inline void CGenerator::block(const std::vector<StmtPtr>& statements) {
    scopes_.emplace_back();
    for (const auto& item : statements) statement(item);
    for (const auto& [name,oldType] : scopes_.back()) {
        if (oldType.empty()) symbols_.erase(name);
        else symbols_[name] = oldType;
    }
    scopes_.pop_back();
}
inline void CGenerator::statement(const StmtPtr& node) {
    switch (node->kind) {
    case Stmt::Kind::Variable: {
        const auto type = node->type.empty() ? inferredType(node->value) : node->type;
        auto& scope = scopes_.back();
        if (!scope.contains(node->name)) {
            auto old = symbols_.find(node->name);
            scope[node->name] = old == symbols_.end() ? "" : old->second;
        }
        symbols_[node->name] = type;
        line(cType(type) + (node->mutable_value ? " " : " const ") +
             cVariableName(node->name) + (node->value ? " = " + expression(node->value) : "") + ";");
        break;
    }
    case Stmt::Kind::Log: {
        const auto type = inferredType(node->value);
        if (type == "string") line("puts(" + expression(node->value) + ");");
        else if (type == "float") line("printf(\"%g\\n\", (double)(" + expression(node->value) + "));");
        else if (type == "int" || type == "bool")
            line("printf(\"%d\\n\", (int)(" + expression(node->value) + "));");
        else fail(node->token,"log expression has no printable type");
        break;
    }
    case Stmt::Kind::Return: line(node->value ? "return " + expression(node->value) + ";" : "return;"); break;
    case Stmt::Kind::Expression: line(expression(node->value) + ";"); break;
    case Stmt::Kind::If:
        line("if (" + expression(node->condition) + ") {"); ++indent_; block(node->body); --indent_;
        if (!node->alternate.empty()) { line("} else {"); ++indent_; block(node->alternate); --indent_; }
        line("}"); break;
    case Stmt::Kind::While:
        line("while (" + expression(node->condition) + ") {"); ++indent_; block(node->body); --indent_; line("}"); break;
    case Stmt::Kind::Break: line("break;"); break;
    case Stmt::Kind::Continue: line("continue;"); break;
    case Stmt::Kind::Pass: line(";"); break;
    default: fail(node->token,"unsupported statement in C generator");
    }
}
inline void CGenerator::function(const StmtPtr& node) {
    symbols_.clear(); scopes_.clear(); scopes_.emplace_back();
    std::string parameters;
    parameters.reserve(node->parameters.size() * 32);
    for (const auto& parameter : node->parameters) {
        if (!parameters.empty()) parameters += ", ";
        parameters += cType(parameter.type) + " const " + cVariableName(parameter.name);
        symbols_[parameter.name] = parameter.type;
        scopes_.back()[parameter.name] = parameter.type;
    }
    line(cType(node->return_type) + " " + cFunctionName(node->name) + "(" +
         (parameters.empty() ? "void" : parameters) + ") {");
    ++indent_; block(node->body); --indent_; line("}"); line();
}
}
