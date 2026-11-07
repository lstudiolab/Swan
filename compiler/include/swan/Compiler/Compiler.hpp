#pragma once
#include "swan/CodeGen/LLVMIRGenerator.hpp"
#include "swan/Frontend/Frontend.hpp"
#include <string>
#include <string_view>

namespace swan {
class Compiler {
public:
    std::string compileIR(std::string_view source, std::string_view target = "native") const {
        auto program = Frontend().build(source);
        const bool size32 = target.find("wasm32") != std::string_view::npos ||
            target.find("armv7") != std::string_view::npos ||
            target.find("armv6") != std::string_view::npos ||
            target.find("i386") != std::string_view::npos ||
            target.find("i686") != std::string_view::npos ||
            target.find("thumb") != std::string_view::npos;
        return LLVMIRGenerator(program, size32).generate();
    }
};
}