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
    output_ << "#include <stdbool.h>\n#include <stdio.h>\n#include <stddef.h>\n\n";
    std::vector<StmtPtr> structures;
    structures.reserve(program_.size());
    functions_.reserve(program_.size());
    std::unordered_map<std::string, StmtPtr> structureByName;
    structureByName.reserve(program_.size());
    for (const auto& node : program_) {
        if (node->kind == Stmt::Kind::Struct) {
            structs_.insert(node->name);
            structures.push_back(node);
            structureByName.emplace(node->name, node);
        }
        if (node->kind == Stmt::Kind::Function) functions_[node->name] = node;
    }

    for (const auto& structure : structures)
        line("typedef struct " + cTypeName(structure->name) + " " + cTypeName(structure->name) + ";");
    if (!structures.empty()) line();

    std::unordered_map<std::string, int> state;
    state.reserve(structures.size());
    std::vector<StmtPtr> ordered;
    ordered.reserve(structures.size());
    std::function<void(const StmtPtr&)> visit = [&](const StmtPtr& structure) {
        int& mark = state[structure->name];
        if (mark == 2) return;
        if (mark == 1) throw CompileError("struct value cycle involving '" + structure->name + "'");
        mark = 1;
        for (const auto& field : structure->fields) {
            if (!structs_.contains(field.type)) continue;
            const auto dependency = structureByName.find(field.type);
            if (dependency != structureByName.end()) visit(dependency->second);
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
        for (const auto& parameter : node->parameters) {
            if (!parameters.empty()) parameters += ", ";
            parameters += cType(parameter.type) + " " + cVariableName(parameter.name);
        }
        line(cType(node->return_type) + " " + cFunctionName(node->name) + "(" +
             (parameters.empty() ? "void" : parameters) + ");");
    }
    line();
    for (const auto& node : program_) if (node->kind == Stmt::Kind::Function) function(node);
    return output_.str();
}
}
