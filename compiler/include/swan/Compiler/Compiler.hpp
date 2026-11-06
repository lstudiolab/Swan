#pragma once
#include "swan/CodeGen/CGenerator.hpp"
#include "swan/Frontend/Frontend.hpp"
#include <string>
#include <string_view>

namespace swan {
class Compiler {
public:
    std::string compile(std::string_view source) const {
        auto program = Frontend().build(source);
        return CGenerator(program).generate();
    }
};
}
