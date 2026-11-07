#pragma once
#include "swan/CodeGen/X86_64Generator.hpp"
#include "swan/Frontend/Frontend.hpp"
#include <string>
#include <string_view>
namespace swan {
class Compiler {
public:
    std::string compileAssembly(std::string_view source,std::string_view target="native") const{
        auto program=Frontend().build(source);
        if(target!="native"&&target!="x86_64"&&target!="x86-64")
            throw CompileError("Swan-owned backend for target '"+std::string(target)+"' is not implemented yet; no LLVM or external compiler backend is used");
        return X86_64Generator(program).generate();
    }
};
}