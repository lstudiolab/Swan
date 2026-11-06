#pragma once

#include "Swan/AST/AST.hpp"

#include <string>
#include <vector>

namespace swan {

struct ParserError {
    SourceLocation location;
    std::string message;
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    Module parse();
    const std::vector<ParserError>& errors() const noexcept;

private:
    const std::vector<Token>& tokens_;
    std::size_t current_ = 0;
    std::vector<ParserError> errors_;

    const Token& peek(std::size_t lookahead = 0) const noexcept;
    const Token& previous() const noexcept;
    bool atEnd() const noexcept;
    bool check(TokenKind kind) const noexcept;
    bool match(TokenKind kind) noexcept;
    const Token& advance() noexcept;
    bool consume(TokenKind kind, const char* message);
    void report(const Token& token, std::string message);
    void synchronize();
    void skipNewlines() noexcept;
    void finishStatement();

    StmtPtr declaration();
    StmtPtr functionDeclaration(bool isAsync, SourceLocation start);
    StmtPtr structDeclaration();
    StmtPtr variableDeclaration();
    StmtPtr ifStatement();
    StmtPtr whileStatement();
    StmtPtr forStatement();
    StmtPtr returnStatement();
    StmtPtr importStatement();
    StmtPtr expressionStatement();
    std::vector<StmtPtr> blockAfterColon();

    ExprPtr expression();
    ExprPtr parsePrecedence(int minimumPrecedence);
    ExprPtr prefixExpression();
    ExprPtr primary();
    ExprPtr postfix(ExprPtr expression);
    int precedence(TokenKind kind) const noexcept;
    bool isAssignment(TokenKind kind) const noexcept;
};

} // namespace swan
