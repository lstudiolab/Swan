#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace swan {
inline CGenerator::CGenerator(const Program& program) : program_(program) {}
inline std::string CGenerator::cType(const std::string& type) const { return mapCType(type, structs_); }
inline std::string CGenerator::quote(const std::string& value) const { return escapeCString(value); }

inline std::string CGenerator::generate() {
    output_.clear();
    output_.reserve(program_.size() * 64 + 64);
    structs_.clear();
    functions_.clear();
    symbols_.clear();
    scopes_.clear();
    indent_ = 0;
    output_ += "#include <stdbool.h>\n#include <stdio.h>\n#include <stddef.h>\n\n";
    std::vector<StmtPtr> structures;
    structures.reserve(program_.size());
    for (const auto& node : program_) {
        if (node->kind == Stmt::Kind::Struct) {
            structs_.insert(node->name);
            structures.push_back(node);
        }
        if (node->kind == Stmt::Kind::Function) functions_[node->name] = node;
    }

    for (const auto& structure : structures)
        line("typedef struct " + cTypeName(structure->name) + " " + cTypeName(structure->name) + ";");
    if (!structures.empty()) line();

    std::unordered_map<std::string, int> state;
    std::vector<StmtPtr> ordered;
    ordered.reserve(structures.size());
    std::function<void(const StmtPtr&)> visit = [&](const StmtPtr& structure) {
        int& mark = state[structure->name];
        if (mark == 2) return;
        if (mark == 1) throw CompileError("struct value cycle involving '" + structure->name + "'");
        mark = 1;
        for (const auto& field : structure->fields) {
            if (!structs_.contains(field.type)) continue;
            auto dependency = std::find_if(structures.begin(), structures.end(),
                [&](const StmtPtr& candidate) { return candidate->name == field.type; });
            if (dependency != structures.end()) visit(*dependency);
        }
        mark = 2;
        ordered.push_back(structure);
    };
    for (const auto& structure : structures) visit(structure);

    for (const auto& node : ordered) {
        line("struct " + cTypeName(node->name) + " {"); ++indent_;
        for (const auto& field : node->fields)
            line(cType(field.type) + " " + cFieldName(field.name) + ";");
        --indent_; line("};"); line();
    }
    for (const auto& node : program_) if (node->kind == Stmt::Kind::Function) {
        std::string parameters;
        parameters.reserve(node->parameters.size() * 32);
        for (const auto& parameter : node->parameters) {
            if (!parameters.empty()) parameters += ", ";
            parameters += cType(parameter.type) + " " + cVariableName(parameter.name);
        }
        line(cType(node->return_type) + " " + cFunctionName(node->name) + "(" +
             (parameters.empty() ? "void" : parameters) + ");");
    }
    line();
    for (const auto& node : program_) if (node->kind == Stmt::Kind::Function) function(node);
    return std::move(output_);
}
}
