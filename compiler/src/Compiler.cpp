#include "swan/Compiler.hpp"
#include "swan/CGenerator.hpp"
#include "swan/Lexer.hpp"
#include "swan/Parser.hpp"
#include "swan/Sema.hpp"

namespace swan {
std::string Compiler::compile(std::string_view source) const {
    auto tokens = Lexer(source).scan();
    auto syntaxTree = Parser(std::move(tokens)).parse();
    Sema(syntaxTree).analyze();
    return CGenerator(syntaxTree).generate();
}
}
