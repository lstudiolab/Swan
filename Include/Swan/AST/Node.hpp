#pragma once

#include "Swan/Frontend/Token.hpp"

#include <memory>

namespace swan {

enum class ASTNodeKind {
    LiteralExpr,
    NameExpr,
    UnaryExpr,
    BinaryExpr,
    MemberExpr,
    CallExpr,
    ArrayExpr,
    ExprStmt,
    VarDecl,
    FunctionDecl,
    StructDecl,
    IfStmt,
    WhileStmt,
    ForStmt,
    ReturnStmt,
    ControlStmt,
    ImportStmt
};

struct ASTNode {
    explicit ASTNode(ASTNodeKind kind, SourceLocation location)
        : kind(kind), location(location) {}
    virtual ~ASTNode() = default;

    ASTNodeKind kind;
    SourceLocation location;
};

struct Expr : ASTNode {
    explicit Expr(ASTNodeKind kind, SourceLocation location)
        : ASTNode(kind, location) {}
};

struct Stmt : ASTNode {
    explicit Stmt(ASTNodeKind kind, SourceLocation location)
        : ASTNode(kind, location) {}
};

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

} // namespace swan
