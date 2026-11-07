#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <functional>
#include <string>
#include <string_view>
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
    indent_ = 0;
    output_ += "#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stddef.h>\n\n";
    std::vector<const Stmt*> structures;
    structures.reserve(program_.size());
    std::unordered_map<std::string_view,const Stmt*> structureByName;
    structureByName.reserve(program_.size());
    for (const auto& node : program_) {
        if (node->kind == Stmt::Kind::Struct) {
            structs_.insert(node->name);
            structureByName.emplace(node->name,node.get());
            structures.push_back(node.get());
        }
    }
    for (const auto* structure : structures)
        line("typedef struct " + cTypeName(structure->name) + " " + cTypeName(structure->name) + ";");
    if (!structures.empty()) line();
    std::unordered_map<std::string_view,unsigned char> state;
    state.reserve(structures.size());
    std::vector<const Stmt*> ordered;
    ordered.reserve(structures.size());
    std::function<void(const Stmt*)> visit = [&](const Stmt* structure) {
        auto& mark = state[structure->name];
        if (mark == 2) return;
        if (mark == 1) throw CompileError("struct value cycle involving '" + structure->name + "'");
        mark = 1;
        for (const auto& field : structure->fields) {
            const auto dependency = structureByName.find(field.type);
            if (dependency != structureByName.end()) visit(dependency->second);
        }
        mark = 2;
        ordered.push_back(structure);
    };
    for (const auto* structure : structures) visit(structure);
    for (const auto* node : ordered) {
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
        const std::string linkage = node->name == "main" ? "" : "static inline ";
        const std::string returnType = node->name == "main" && node->return_type == "int"
            ? "int" : cType(node->return_type);
        line(linkage + returnType + " " + cFunctionName(node->name) + "(" +
             (parameters.empty() ? "void" : parameters) + ");");
    }
    line();
    for (const auto& node : program_) if (node->kind == Stmt::Kind::Function) function(node);
    return std::move(output_);
}
}
