#pragma once
#include "swan/AST/AST.hpp"
#include "swan/Lex/Lexer.hpp"
#include "swan/Parse/Parser.hpp"
#include "swan/Sema/Sema.hpp"
#include <string_view>
#include <utility>

namespace swan {
class Frontend {
public:
    Program build(std::string_view source) const {
        auto tokens = Lexer(source).scan();
        auto syntaxTree = Parser(std::move(tokens)).parse();
        Sema(syntaxTree).analyze();
        return syntaxTree;
    }
};
}
