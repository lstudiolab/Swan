#pragma once
#include "swan/Sema/Sema.hpp"
#include <string>
#include <utility>

namespace swan {
inline void Sema::analyze() {
    functions_.reserve(program_.size());
    types_.reserve(types_.size() + program_.size());
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
            names.reserve(node->fields.size());
            for (const auto& field : node->fields) {
                if (!types_.contains(field.type)) fail(field.token,"unknown field type '" + field.type + "'");
                if (field.type == "void") fail(field.token,"struct fields cannot have type void");
                if (!names.insert(field.name).second) fail(field.token,"duplicate field '" + field.name + "'");
            }
        } else if (node->kind == Stmt::Kind::Function) analyzeFunction(node);
        else fail(node->token,"only function and struct declarations are allowed at file scope");
    }
}

inline void Sema::analyzeFunction(const StmtPtr& function) {
    scopes_.clear(); scopes_.emplace_back();
    scopes_.back().reserve(function->parameters.size());
    return_type_ = function->return_type;
    if (!types_.contains(return_type_)) fail(function->token,"unknown return type '" + return_type_ + "'");
    for (const auto& parameter : function->parameters) {
        if (parameter.type.empty()) fail(parameter.token,"parameter '" + parameter.name + "' requires a type");
        if (!types_.contains(parameter.type)) fail(parameter.token,"unknown parameter type '" + parameter.type + "'");
        if (parameter.type == "void") fail(parameter.token,"parameters cannot have type void");
        if (!scopes_.back().emplace(parameter.name,std::pair{parameter.type,false}).second)
            fail(parameter.token,"duplicate parameter '" + parameter.name + "'");
    }
    loop_depth_ = 0;
    analyzeBlock(function->body);
    if (return_type_ != "void" && !alwaysReturns(function->body))
        fail(function->token,"non-void function '" + function->name + "' does not return on every path");
}
}
