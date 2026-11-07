#pragma once
#include "swan/CodeGen/CGenerator.hpp"
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
    std::unordered_map<std::string_view, const Stmt*> structureByName;
    structureByName.reserve(program_.size());
    for (const auto& node : program_) {
        if (node->kind == Stmt::Kind::Struct) {
            structs_.insert(node->name);
            structureByName.emplace(node->name, node.get());
            structures.push_back(node.get());
        }
    }

    for (const auto* structure : structures) {
        beginLine();
        output_ += "typedef struct ";
        output_ += cTypeName(structure->name);
        output_.push_back(' ');
        output_ += cTypeName(structure->name);
        output_.push_back(';');
        endLine();
    }
    if (!structures.empty()) line();

    std::unordered_map<std::string_view, unsigned char> state;
    state.reserve(structures.size());
    std::vector<const Stmt*> ordered;
    ordered.reserve(structures.size());

    // Generic-lambda recursion avoids std::function type erasure and its
    // possible heap allocation in this compile-time dependency walk.
    const auto visit = [&](const auto& self, const Stmt* structure) -> void {
        auto& mark = state[structure->name];
        if (mark == 2) return;
        if (mark == 1)
            throw CompileError("struct value cycle involving '" + structure->name + "'");
        mark = 1;
        for (const auto& field : structure->fields) {
            const auto dependency = structureByName.find(field.type);
            if (dependency != structureByName.end())
                self(self, dependency->second);
        }
        mark = 2;
        ordered.push_back(structure);
    };
    for (const auto* structure : structures) visit(visit, structure);

    for (const auto* node : ordered) {
        beginLine();
        output_ += "struct ";
        output_ += cTypeName(node->name);
        output_ += " {";
        endLine();
        ++indent_;
        for (const auto& field : node->fields) {
            beginLine();
            output_ += cType(field.type);
            output_.push_back(' ');
            output_ += cFieldName(field.name);
            output_.push_back(';');
            endLine();
        }
        --indent_;
        line("};");
        line();
    }

    for (const auto& node : program_) {
        if (node->kind != Stmt::Kind::Function) continue;
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
                output_.push_back(' ');
                output_ += cVariableName(parameter.name);
            }
        }
        output_ += ");";
        endLine();
    }
    line();

    for (const auto& node : program_)
        if (node->kind == Stmt::Kind::Function) function(node);
    return std::move(output_);
}
}
