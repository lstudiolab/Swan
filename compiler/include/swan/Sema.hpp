#pragma once
#include "swan/AST.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace swan {
class Sema {
public:
    explicit Sema(const Program& program);
    void analyze();
private:
    using Scope = std::unordered_map<std::string,std::pair<std::string,bool>>;
    void analyzeFunction(const StmtPtr& function);
    void analyzeStatement(const StmtPtr& statement);
    void analyzeBlock(const std::vector<StmtPtr>& statements);
    std::string analyzeExpression(const ExprPtr& expression);
    std::pair<std::string,bool>* lookup(const std::string& name);
    static bool numeric(const std::string& type);
    static bool compatible(const std::string& expected,const std::string& actual);
    const Program& program_;
    std::unordered_set<std::string> types_{"int","float","bool","string","void"};
    std::unordered_map<std::string,StmtPtr> functions_;
    std::vector<Scope> scopes_;
    std::string return_type_;
    unsigned loop_depth_ = 0;
};
}

namespace swan {
inline Sema::Sema(const Program& program) : program_(program) {}
inline bool inline Sema::numeric(const std::string& type) { return type == "int" || type == "float"; }
inline bool inline Sema::compatible(const std::string& expected,const std::string& actual) {
    return expected == actual || (expected == "float" && actual == "int");
}
inline std::pair<std::string,bool>* inline Sema::lookup(const std::string& name) {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) return &found->second;
    }
    return nullptr;
}
inline void inline Sema::analyze() {
    for (const auto& node : program_) {
        if (node->kind == Stmt::Kind::Struct && !types_.insert(node->name).second)
            fail(node->token,"type '" + node->name + "' is already defined");
        if (node->kind == Stmt::Kind::Function &&
            !functions_.emplace(node->name,node).second)
            fail(node->token,"function '" + node->name + "' is already declared");
    }
    for (const auto& node : program_) {
        if (node->kind == Stmt::Kind::Struct) {
            std::unordered_set<std::string> names;
            for (const auto& field : node->fields) {
                if (!types_.contains(field.type)) fail(field.token,"unknown field type '" + field.type + "'");
                if (!names.insert(field.name).second) fail(field.token,"duplicate field '" + field.name + "'");
            }
        } else if (node->kind == Stmt::Kind::Function) analyzeFunction(node);
        else fail(node->token,"only function and struct declarations are allowed at file scope");
    }
}
inline void inline Sema::analyzeFunction(const StmtPtr& function) {
    scopes_.clear(); scopes_.emplace_back();
    return_type_ = function->return_type;
    if (!types_.contains(return_type_)) fail(function->token,"unknown return type '" + return_type_ + "'");
    for (const auto& parameter : function->parameters) {
        if (parameter.type.empty()) fail(parameter.token,"parameter '" + parameter.name + "' requires a type");
        if (!types_.contains(parameter.type)) fail(parameter.token,"unknown parameter type '" + parameter.type + "'");
        if (!scopes_.back().emplace(parameter.name,std::pair{parameter.type,false}).second)
            fail(parameter.token,"duplicate parameter '" + parameter.name + "'");
    }
    loop_depth_ = 0;
    analyzeBlock(function->body);
}
inline void inline Sema::analyzeBlock(const std::vector<StmtPtr>& statements) {
    scopes_.emplace_back();
    for (const auto& statement : statements) analyzeStatement(statement);
    scopes_.pop_back();
}
inline void inline Sema::analyzeStatement(const StmtPtr& node) {
    switch (node->kind) {
    case Stmt::Kind::Variable: {
        if (scopes_.back().contains(node->name))
            fail(node->token,"'" + node->name + "' is already declared in this scope");
        std::string actual = node->value ? analyzeExpression(node->value) : "";
        std::string type = node->type.empty() ? actual : node->type;
        if (type.empty()) fail(node->token,"cannot infer type for '" + node->name + "'");
        if (!types_.contains(type)) fail(node->token,"unknown type '" + type + "'");
        if (!actual.empty() && !compatible(type,actual))
            fail(node->token,"cannot initialize " + type + " with " + actual);
        scopes_.back()[node->name] = {type,node->mutable_value};
        break;
    }
    case Stmt::Kind::Log: {
        const auto type = analyzeExpression(node->value);
        if (type != "string" && type != "bool" && !numeric(type))
            fail(node->token,"log expects a string, number, or boolean");
        break;
    }
    case Stmt::Kind::Return: {
        const auto actual = node->value ? analyzeExpression(node->value) : "void";
        if (!compatible(return_type_,actual))
            fail(node->token,"function returns " + return_type_ + ", but this return has type " + actual);
        break;
    }
    case Stmt::Kind::Expression: analyzeExpression(node->value); break;
    case Stmt::Kind::If:
        if (analyzeExpression(node->condition) != "bool") fail(node->token,"if condition must be bool");
        analyzeBlock(node->body);
        if (!node->alternate.empty()) analyzeBlock(node->alternate);
        break;
    case Stmt::Kind::While:
        if (analyzeExpression(node->condition) != "bool") fail(node->token,"while condition must be bool");
        ++loop_depth_; analyzeBlock(node->body); --loop_depth_;
        break;
    case Stmt::Kind::Break:
    case Stmt::Kind::Continue:
        if (loop_depth_ == 0) fail(node->token,"loop control used outside a loop");
        break;
    case Stmt::Kind::Pass: break;
    default: fail(node->token,"statement is not supported in this context");
    }
}
inline std::string inline Sema::analyzeExpression(const ExprPtr& node) {
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

