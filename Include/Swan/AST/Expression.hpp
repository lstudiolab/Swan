#pragma once

#include "Swan/AST/Node.hpp"

#include <string>
#include <utility>
#include <vector>

namespace swan {

struct LiteralExpr final : Expr {
    explicit LiteralExpr(Token token)
        : Expr(ASTNodeKind::LiteralExpr, token.location), token(std::move(token)) {}
    Token token;
};

struct NameExpr final : Expr {
    NameExpr(std::string name, SourceLocation location)
        : Expr(ASTNodeKind::NameExpr, location), name(std::move(name)) {}
    std::string name;
};

struct UnaryExpr final : Expr {
    UnaryExpr(Token op, ExprPtr operand)
        : Expr(ASTNodeKind::UnaryExpr, op.location),
          op(std::move(op)), operand(std::move(operand)) {}
    Token op;
    ExprPtr operand;
};

struct BinaryExpr final : Expr {
    BinaryExpr(ExprPtr left, Token op, ExprPtr right)
        : Expr(ASTNodeKind::BinaryExpr, op.location),
          left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
    ExprPtr left;
    Token op;
    ExprPtr right;
};

struct MemberExpr final : Expr {
    MemberExpr(ExprPtr base, Token member)
        : Expr(ASTNodeKind::MemberExpr, member.location),
          base(std::move(base)), member(std::move(member)) {}
    ExprPtr base;
    Token member;
};

struct CallExpr final : Expr {
    CallExpr(ExprPtr callee, std::vector<ExprPtr> arguments, SourceLocation location)
        : Expr(ASTNodeKind::CallExpr, location),
          callee(std::move(callee)), arguments(std::move(arguments)) {}
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
};

struct ArrayExpr final : Expr {
    ArrayExpr(std::vector<ExprPtr> elements, SourceLocation location)
        : Expr(ASTNodeKind::ArrayExpr, location), elements(std::move(elements)) {}
    std::vector<ExprPtr> elements;
};

} // namespace swan
