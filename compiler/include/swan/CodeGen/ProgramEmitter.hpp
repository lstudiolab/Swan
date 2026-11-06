#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <string>
#include <utility>

namespace swan {
inline CGenerator::CGenerator(const Program& program) : program_(program) {}

inline std::string CGenerator::cType(const std::string& type) const {
    return mapCType(type, structs_);
}

inline std::string CGenerator::quote(const std::string& value) const { return escapeCString(value); }

inline std::string CGenerator::generate() {
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
