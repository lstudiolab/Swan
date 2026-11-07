#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <string>
#include <utility>

namespace swan {
inline void CGenerator::beginLine() {
    output_.append(static_cast<std::size_t>(indent_) * 4, ' ');
}
inline void CGenerator::endLine() { output_.push_back('\n'); }
inline void CGenerator::line(const std::string& value) {
    if (!value.empty()) {
        beginLine();
        output_.append(value);
    }
    endLine();
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
        beginLine();
        appendCType(output_, type);
        output_ += node->mutable_value ? " " : " const ";
        output_ += "swan_var_";
        output_ += node->name;
        if (node->value) {
            output_ += " = ";
            appendExpression(output_, node->value);
        }
        output_.push_back(';');
        endLine();
        break;
    }
    case Stmt::Kind::Log: {
        const auto type = inferredType(node->value);
        beginLine();
        if (type == "string") {
            output_ += "puts(";
            appendExpression(output_, node->value);
            output_ += ");";
        } else if (type == "float" || type == "int" || type == "bool") {
            output_ += type == "float" ? "printf(\"%g\\n\", (double)("
                                          : "printf(\"%d\\n\", (int)(";
            appendExpression(output_, node->value);
            output_ += "));";
        } else {
            fail(node->token,"log expression has no printable type");
        }
        endLine();
        break;
    }
    case Stmt::Kind::Return:
        beginLine();
        output_ += "return";
        if (node->value) {
            output_.push_back(' ');
            appendExpression(output_, node->value);
        }
        output_.push_back(';');
        endLine();
        break;
    case Stmt::Kind::Expression:
        beginLine();
        appendExpression(output_, node->value);
        output_.push_back(';');
        endLine();
        break;
    case Stmt::Kind::If:
        beginLine();
        output_ += "if (";
        appendExpression(output_, node->condition);
        output_ += ") {";
        endLine();
        ++indent_;
        block(node->body);
        --indent_;
        if (!node->alternate.empty()) {
            beginLine(); output_ += "} else {"; endLine();
            ++indent_;
            block(node->alternate);
            --indent_;
        }
        line("}");
        break;
    case Stmt::Kind::While:
        beginLine();
        output_ += "while (";
        appendExpression(output_, node->condition);
        output_ += ") {";
        endLine();
        ++indent_;
        block(node->body);
        --indent_;
        line("}");
        break;
    case Stmt::Kind::Break: line("break;"); break;
    case Stmt::Kind::Continue: line("continue;"); break;
    case Stmt::Kind::Pass: line(";"); break;
    default: fail(node->token,"unsupported statement in C generator");
    }
}
inline void CGenerator::function(const StmtPtr& node) {
    symbols_.clear();
    scopes_.clear();
    scopes_.emplace_back();
    beginLine();
    appendCType(output_, node->return_type);
    output_.push_back(' ');
    output_ += node->name == "main" ? "main" : "swan_fn_";
    if (node->name != "main") output_ += node->name;
    output_.push_back('(');
    if (node->parameters.empty()) {
        output_ += "void";
    } else {
        bool first = true;
        for (const auto& parameter : node->parameters) {
            if (!first) output_ += ", ";
            first = false;
            appendCType(output_, parameter.type);
            output_ += " const swan_var_";
            output_ += parameter.name;
            symbols_[parameter.name] = parameter.type;
            scopes_.back()[parameter.name] = parameter.type;
        }
    }
    output_ += ") {";
    endLine();
    ++indent_;
    block(node->body);
    --indent_;
    line("}");
    line();
}
}
