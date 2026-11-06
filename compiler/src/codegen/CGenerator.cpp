#include "swan/CGenerator.hpp"
#include "swan/Token.hpp"
#include <sstream>

namespace swan {
CGenerator::CGenerator(const Program& program) : program_(program) {}
std::string CGenerator::cType(const std::string& type) const {
    if (type == "int") return "int";
    if (type == "float") return "double";
    if (type == "bool") return "bool";
    if (type == "string") return "const char *";
    if (type == "void") return "void";
    if (structs_.contains(type)) return type;
    throw CompileError("unknown C type '" + type + "'");
}
void CGenerator::line(const std::string& value) {
    if (!value.empty()) output_ << std::string(indent_ * 4,' ') << value;
    output_ << '\n';
}
std::string CGenerator::quote(const std::string& value) const {
    std::ostringstream result; result << '"';
    for (unsigned char ch : value) {
        switch (ch) {
            case '\\': result << "\\\\"; break;
            case '"': result << "\\\""; break;
            case '\n': result << "\\n"; break;
            case '\r': result << "\\r"; break;
            case '\t': result << "\\t"; break;
            default:
                if (ch < 32 || ch >= 127) {
                    const char* hex = "0123456789abcdef";
                    result << "\\x" << hex[ch >> 4] << hex[ch & 15] << "\"\"";
                } else result << static_cast<char>(ch);
        }
    }
    result << '"'; return result.str();
}
std::string CGenerator::inferredType(const ExprPtr& node) const {
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
std::string CGenerator::expression(const ExprPtr& node) const {
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
void CGenerator::block(const std::vector<StmtPtr>& statements) {
    scopes_.emplace_back();
    for (const auto& item : statements) statement(item);
    for (const auto& [name,oldType] : scopes_.back()) {
        if (oldType.empty()) symbols_.erase(name);
        else symbols_[name] = oldType;
    }
    scopes_.pop_back();
}
void CGenerator::statement(const StmtPtr& node) {
    switch (node->kind) {
    case Stmt::Kind::Variable: {
        const auto type = node->type.empty() ? inferredType(node->value) : node->type;
        auto& scope = scopes_.back();
        if (!scope.contains(node->name)) {
            auto old = symbols_.find(node->name);
            scope[node->name] = old == symbols_.end() ? "" : old->second;
        }
        symbols_[node->name] = type;
        line(std::string(node->mutable_value ? "" : "const ") + cType(type) + " " + node->name +
             (node->value ? " = " + expression(node->value) : "") + ";");
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
    case Stmt::Kind::Return:
        line(node->value ? "return " + expression(node->value) + ";" : "return;");
        break;
    case Stmt::Kind::Expression: line(expression(node->value) + ";"); break;
    case Stmt::Kind::If:
        line("if (" + expression(node->condition) + ") {"); ++indent_; block(node->body); --indent_;
        if (!node->alternate.empty()) {
            line("} else {"); ++indent_; block(node->alternate); --indent_;
        }
        line("}");
        break;
    case Stmt::Kind::While:
        line("while (" + expression(node->condition) + ") {"); ++indent_; block(node->body); --indent_; line("}");
        break;
    case Stmt::Kind::Break: line("break;"); break;
    case Stmt::Kind::Continue: line("continue;"); break;
    case Stmt::Kind::Pass: line(";"); break;
    default: fail(node->token,"unsupported statement in C generator");
    }
}
void CGenerator::function(const StmtPtr& node) {
    symbols_.clear(); scopes_.clear(); scopes_.emplace_back();
    std::string parameters;
    for (const auto& parameter : node->parameters) {
        if (!parameters.empty()) parameters += ", ";
        parameters += cType(parameter.type) + " " + parameter.name;
        symbols_[parameter.name] = parameter.type;
        scopes_.back()[parameter.name] = parameter.type;
    }
    line(cType(node->return_type) + " " + node->name + "(" +
         (parameters.empty() ? "void" : parameters) + ") {");
    ++indent_; block(node->body); --indent_; line("}"); line();
}
std::string CGenerator::generate() {
    output_ << "#include <stdbool.h>\n#include <stdio.h>\n#include <stddef.h>\n\n";
    for (const auto& node : program_) {
        if (node->kind == Stmt::Kind::Struct) structs_.insert(node->name);
        if (node->kind == Stmt::Kind::Function) functions_[node->name] = node;
    }
    for (const auto& node : program_) if (node->kind == Stmt::Kind::Struct) {
        line("typedef struct " + node->name + " {"); ++indent_;
        for (const auto& field : node->fields) line(cType(field.type) + " " + field.name + ";");
        --indent_; line("} " + node->name + ";"); line();
    }
    for (const auto& node : program_) if (node->kind == Stmt::Kind::Function) {
        std::string parameters;
        for (const auto& parameter : node->parameters) {
            if (!parameters.empty()) parameters += ", ";
            parameters += cType(parameter.type) + " " + parameter.name;
        }
        line(cType(node->return_type) + " " + node->name + "(" +
             (parameters.empty() ? "void" : parameters) + ");");
    }
    line();
    for (const auto& node : program_) if (node->kind == Stmt::Kind::Function) function(node);
    return output_.str();
}
}
