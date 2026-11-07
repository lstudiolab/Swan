#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace swan {
inline CGenerator::CGenerator(const Program& program) : program_(program) {}
inline std::string CGenerator::cType(const std::string& type) const { return mapCType(type, structs_); }
inline void CGenerator::appendCType(std::string& output, std::string_view type) const {
    appendMappedCType(output, type, structs_);
}
inline std::string CGenerator::quote(const std::string& value) const { return escapeCString(value); }

inline std::string CGenerator::generate() {
    output_.reserve(std::max<std::size_t>(4096, program_.size() * 96));
    output_ += "#include <stdbool.h>\n#include <stdio.h>\n#include <stddef.h>\n\n";
    std::vector<StmtPtr> structures;
    structures.reserve(program_.size());
    functions_.reserve(program_.size());
    StringViewMap<StmtPtr> structureByName;
    structureByName.reserve(program_.size());
    for (const auto& node : program_) {
        if (node->kind == Stmt::Kind::Struct) {
            structs_.insert(node->name);
            structures.push_back(node);
            structureByName.emplace(node->name, node);
        }
        if (node->kind == Stmt::Kind::Function) functions_.emplace(node->name, node);
    }

    for (const auto& structure : structures) {
        beginLine();
        output_ += "typedef struct swan_type_";
        output_ += structure->name;
        output_ += " swan_type_";
        output_ += structure->name;
        output_.push_back(';');
        endLine();
    }
    if (!structures.empty()) line();

    StringViewMap<unsigned char> state;
    state.reserve(structures.size());
    std::vector<StmtPtr> ordered;
    ordered.reserve(structures.size());
    const auto visit = [&](const auto& self, const StmtPtr& structure) -> void {
        auto& mark = state[structure->name];
        if (mark == 2) return;
        if (mark == 1) throw CompileError("struct value cycle involving '" + structure->name + "'");
        mark = 1;
        for (const auto& field : structure->fields) {
            if (!structs_.contains(field.type)) continue;
            const auto dependency = structureByName.find(field.type);
            if (dependency != structureByName.end()) self(self, dependency->second);
        }
        mark = 2;
        ordered.push_back(structure);
    };
    for (const auto& structure : structures) visit(visit, structure);

    for (const auto& node : ordered) {
        beginLine();
        output_ += "struct swan_type_";
        output_ += node->name;
        output_ += " {";
        endLine();
        ++indent_;
        for (const auto& field : node->fields) {
            beginLine();
            appendCType(output_, field.type);
            output_ += " swan_field_";
            output_ += field.name;
            output_.push_back(';');
            endLine();
        }
        --indent_;
        line("};");
        line();
    }
    for (const auto& node : program_) {
        if (node->kind != Stmt::Kind::Function) continue;
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
                output_ += " swan_var_";
                output_ += parameter.name;
            }
        }
        output_ += ");";
        endLine();
    }
    line();
    for (const auto& node : program_) {
        if (node->kind == Stmt::Kind::Function) function(node);
    }
    return std::move(output_);
}
}
