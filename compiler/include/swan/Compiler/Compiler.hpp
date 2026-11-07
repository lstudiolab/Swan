#pragma once
#include "swan/CodeGen/X86_64Generator.hpp"
#include "swan/CodeGen/AArch64Generator.hpp"
#include "swan/Frontend/Frontend.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
namespace swan {
class Compiler {
    static void validateStructCycles(const Program& program) {
        std::unordered_map<std::string,const Stmt*> structures;
        std::unordered_map<std::string,unsigned char> marks;
        for(const auto& node:program)if(node->kind==Stmt::Kind::Struct)structures.emplace(node->name,node.get());
        const auto visit=[&](const auto& self,const Stmt* structure)->void {
            auto& mark=marks[structure->name];if(mark==2)return;
            if(mark==1)throw CompileError("struct value cycle involving '"+structure->name+"'");
            mark=1;
            for(const auto& field:structure->fields){auto found=structures.find(field.type);if(found!=structures.end())self(self,found->second);}
            mark=2;
        };
        for(const auto& entry:structures)visit(visit,entry.second);
    }
public:
    std::string compileAssembly(std::string_view source,std::string_view target="native") const {
        auto program=Frontend().build(source);
        validateStructCycles(program);
        if(target=="native"||target=="x86_64"||target=="x86-64")return X86_64Generator(program).generate();
        if(target=="aarch64"||target=="arm64"||target=="aarch64-unknown-linux-gnu")return AArch64Generator(program).generate();
        throw CompileError("Swan-owned backend for target '"+std::string(target)+"' is not implemented yet; no LLVM or external compiler backend is used");
    }
};
}