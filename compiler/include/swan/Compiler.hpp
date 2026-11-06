#pragma once
#include "swan/Lexer.hpp"
#include "swan/Parser.hpp"
#include "swan/Sema.hpp"
#include "swan/CGenerator.hpp"
#include <string>
#include <string_view>
#include <utility>

namespace swan {
class Compiler {
public:
    std::string compile(std::string_view source) const {
        auto tokens = Lexer(source).scan();
        auto syntaxTree = Parser(std::move(tokens)).parse();
        Sema(syntaxTree).analyze();
        return CGenerator(syntaxTree).generate();
    }
};
}
