#pragma once

#include "Swan/Frontend/Token.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace swan {

struct Expr {
    explicit Expr(SourceLocation location) : location(location) {}
    virtual ~Expr() = default;
    SourceLocation location;
};
using ExprPtr = std::unique_ptr<Expr>;

struct LiteralExpr final : Expr {
    LiteralExpr(Token token) : Expr(token.location), token(std::move(token)) {}
    Token token;
};
struct NameExpr final : Expr {
    NameExpr(std::string name, SourceLocation location)
        : Expr(location), name(std::move(name)) {}
    std::string name;
};
struct UnaryExpr final : Expr {
    UnaryExpr(Token op, ExprPtr operand)
        : Expr(op.location), op(std::move(op)), operand(std::move(operand)) {}
    Token op;
    ExprPtr operand;
};
struct BinaryExpr final : Expr {
    BinaryExpr(ExprPtr left, Token op, ExprPtr right)
        : Expr(op.location), left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
    ExprPtr left;
    Token op;
    ExprPtr right;
};
struct MemberExpr final : Expr {
    MemberExpr(ExprPtr base, Token member)
        : Expr(member.location), base(std::move(base)), member(std::move(member)) {}
    ExprPtr base;
    Token member;
};
struct CallExpr final : Expr {
    CallExpr(ExprPtr callee, std::vector<ExprPtr> arguments, SourceLocation location)
        : Expr(location), callee(std::move(callee)), arguments(std::move(arguments)) {}
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
};
struct ArrayExpr final : Expr {
    ArrayExpr(std::vector<ExprPtr> elements, SourceLocation location)
        : Expr(location), elements(std::move(elements)) {}
    std::vector<ExprPtr> elements;
};

struct Stmt {
    explicit Stmt(SourceLocation location) : location(location) {}
    virtual ~Stmt() = default;
    SourceLocation location;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct ExprStmt final : Stmt {
    ExprStmt(ExprPtr expression, SourceLocation location)
        : Stmt(location), expression(std::move(expression)) {}
    ExprPtr expression;
};
struct VarDecl final : Stmt {
    VarDecl(TokenKind declarationKind, std::string name, std::optional<std::string> typeName,
            ExprPtr initializer, SourceLocation location)
        : Stmt(location), declarationKind(declarationKind), name(std::move(name)),
          typeName(std::move(typeName)), initializer(std::move(initializer)) {}
    TokenKind declarationKind;
    std::string name;
    std::optional<std::string> typeName;
    ExprPtr initializer;
};
struct FunctionDecl final : Stmt {
    FunctionDecl(std::string name, std::vector<std::string> parameters,
                 std::optional<std::string> returnType, std::vector<StmtPtr> body,
                 bool isAsync, SourceLocation location)
        : Stmt(location), name(std::move(name)), parameters(std::move(parameters)),
          returnType(std::move(returnType)), body(std::move(body)), isAsync(isAsync) {}
    std::string name;
    std::vector<std::string> parameters;
    std::optional<std::string> returnType;
    std::vector<StmtPtr> body;
    bool isAsync;
};
struct StructDecl final : Stmt {
    StructDecl(std::string name, std::vector<StmtPtr> members, SourceLocation location)
        : Stmt(location), name(std::move(name)), members(std::move(members)) {}
    std::string name;
    std::vector<StmtPtr> members;
};
struct IfStmt final : Stmt {
    IfStmt(ExprPtr condition, std::vector<StmtPtr> body, std::vector<StmtPtr> elseBody,
           SourceLocation location)
        : Stmt(location), condition(std::move(condition)), body(std::move(body)),
          elseBody(std::move(elseBody)) {}
    ExprPtr condition;
    std::vector<StmtPtr> body;
    std::vector<StmtPtr> elseBody;
};
struct WhileStmt final : Stmt {
    WhileStmt(ExprPtr condition, std::vector<StmtPtr> body, SourceLocation location)
        : Stmt(location), condition(std::move(condition)), body(std::move(body)) {}
    ExprPtr condition;
    std::vector<StmtPtr> body;
};
struct ForStmt final : Stmt {
    ForStmt(std::string variable, ExprPtr sequence, std::vector<StmtPtr> body,
            SourceLocation location)
        : Stmt(location), variable(std::move(variable)), sequence(std::move(sequence)),
          body(std::move(body)) {}
    std::string variable;
    ExprPtr sequence;
    std::vector<StmtPtr> body;
};
struct ReturnStmt final : Stmt {
    ReturnStmt(ExprPtr value, SourceLocation location)
        : Stmt(location), value(std::move(value)) {}
    ExprPtr value;
};
struct ControlStmt final : Stmt {
    ControlStmt(TokenKind kind, SourceLocation location) : Stmt(location), kind(kind) {}
    TokenKind kind;
};
struct ImportStmt final : Stmt {
    ImportStmt(std::string module, std::optional<std::string> alias, SourceLocation location)
        : Stmt(location), module(std::move(module)), alias(std::move(alias)) {}
    std::string module;
    std::optional<std::string> alias;
};

struct Module {
    std::vector<StmtPtr> statements;
};

} // namespace swan
