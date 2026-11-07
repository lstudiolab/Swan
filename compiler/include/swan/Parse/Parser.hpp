#pragma once
#include "swan/AST/AST.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include "swan/Parse/Precedence.hpp"
#include <vector>

namespace swan {
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();
private:
    const Token& peek(std::size_t offset = 0) const;
    bool check(std::string_view kind) const;
    bool match(std::string_view kind);
    Token& advance();
    Token& expect(std::string_view kind, std::string_view message = {});
    void separators();
    std::string typeName();
    std::vector<StmtPtr> block();
    StmtPtr declaration();
    StmtPtr statement();
    ExprPtr expression(int minimum = 1);
    ExprPtr prefix();
    static int precedence(std::string_view op);
    std::vector<Token> tokens_;
    std::size_t position_ = 0;
};
}

#include "swan/Parse/ParserCore.hpp"
#include "swan/Parse/ParserDeclarations.hpp"
#include "swan/Parse/ParserStatements.hpp"
#include "swan/Parse/ParserExpressions.hpp"
