#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <string>
#include <string_view>
#include <utility>

namespace swan {
inline void CGenerator::beginLine() {
    output_.append(indent_ * 4, ' ');
}
inline void CGenerator::endLine() {
    output_.push_back('\\n');
}
inline void CGenerator::line(const std::string& value) {
    if (!value.empty()) {
        beginLine();
        output_.append(value);
    }
    endLine();
}
inline void CGenerator::block(const std::vector<StmtPtr>& statements) {
    for (const auto& item : statements) statement(item);
}
inline void CGenerator::statement(const StmtPtr& node) {
    switch (node->kind) {
    case Stmt::Kind::Variable: {
        const auto type = node->type.empty() ? inferredType(node->value) : std::string_view(node->type);
        beginLine();
        output_ += cType(std::string(type));
        output_.push_back(' ');
        if (!node->mutable_value) output_ += "const ";
        output_ += cVariableName(node->name);
        if (node->value) {
            output_ += " = ";
            appendExpression(node->value, output_);
        }
        output_ += ";";
        endLine();
        break;
    }
    case Stmt::Kind::Log: {
        const auto type = inferredType(node->value);
        beginLine();
        if (type == "string") {
            output_ += "puts(";
            appendExpression(node->value, output_);
            output_ += ");";
        } else if (type == "float") {
            output_ += "printf(\"%g\\n\", (double)(";
            appendExpression(node->value, output_);
            output_ += "));";
        } else if (type == "int") {
            output_ += "printf(\"%lld\\n\", (long long)(";
            appendExpression(node->value, output_);
            output_ += "));";
        } else if (type == "bool") {
            output_ += "printf(\"%d\\n\", (int)(";
            appendExpression(node->value, output_);
            output_ += "));";
        } else {
            fail(node->token, "log expression has no printable type");
        }
        endLine();
        break;
    }
    case Stmt::Kind::Return:
        beginLine();
        output_ += "return";
        if (node->value) {
            output_.push_back(' ');
            appendExpression(node->value, output_);
        }
        output_.push_back(';');
        endLine();
        break;
    case Stmt::Kind::Expression:
        beginLine();
        appendExpression(node->value, output_);
        output_.push_back(';');
        endLine();
        break;
    case Stmt::Kind::If:
        beginLine();
        output_ += "if (";
        appendExpression(node->condition, output_);
        output_ += ") {";
        endLine();
        ++indent_;
        block(node->body);
        --indent_;
        if (!node->alternate.empty()) {
            line("} else {");
            ++indent_;
            block(node->alternate);
            --indent_;
        }
        line("}");
        break;
    case Stmt::Kind::While:
        beginLine();
        output_ += "while (";
        appendExpression(node->condition, output_);
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
    default: fail(node->token, "unsupported statement in C generator");
    }
}
inline void CGenerator::function(const StmtPtr& node) {
    const std::string linkage = node->name == "main" ? "" : "static inline ";
    const std::string returnType = node->name == "main" && node->return_type == "int"
        ? "int" : cType(node->return_type);
    beginLine();
    output_ += linkage;
    output_ += returnType;
    output_.push_back(' ');
    output_ += cFunctionName(node->name);
    output_.push_back('(');
    if (node->parameters.empty()) {
        output_ += "void";
    } else {
        for (std::size_t i = 0; i < node->parameters.size(); ++i) {
            if (i) output_ += ", ";
            const auto& parameter = node->parameters[i];
            output_ += cType(parameter.type);
            output_ += " const ";
            output_ += cVariableName(parameter.name);
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
