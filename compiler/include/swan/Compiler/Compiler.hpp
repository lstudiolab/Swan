#pragma once
#include "swan/CodeGen/LLVMIRGenerator.hpp"
#include "swan/Frontend/Frontend.hpp"
#include <string>
#include <string_view>

namespace swan {
class Compiler {
public:
    std::string compileIR(std::string_view source) const {
        auto program = Frontend().build(source);
        return LLVMIRGenerator(program).generate();
    }
};
}