#pragma once

#include "Swan/AST/Node.hpp"

#include <optional>
#include <string>
#include <utility>

namespace swan {

struct ExprStmt final : Stmt {
    ExprStmt(ExprPtr expression, SourceLocation location)
        : Stmt(ASTNodeKind::ExprStmt, location), expression(std::move(expression)) {}
    ExprPtr expression;
};

struct VarDecl final : Stmt {
    VarDecl(TokenKind declarationKind, std::string name,
            std::optional<std::string> typeName, ExprPtr initializer,
            SourceLocation location)
        : Stmt(ASTNodeKind::VarDecl, location),
          declarationKind(declarationKind), name(std::move(name)),
          typeName(std::move(typeName)), initializer(std::move(initializer)) {}

    TokenKind declarationKind;
    std::string name;
    std::optional<std::string> typeName;
    ExprPtr initializer;
};

struct ReturnStmt final : Stmt {
    ReturnStmt(ExprPtr value, SourceLocation location)
        : Stmt(ASTNodeKind::ReturnStmt, location), value(std::move(value)) {}
    ExprPtr value;
};

struct ControlStmt final : Stmt {
    ControlStmt(TokenKind kind, SourceLocation location)
        : Stmt(ASTNodeKind::ControlStmt, location), controlKind(kind) {}
    TokenKind controlKind;
};

struct ImportStmt final : Stmt {
    ImportStmt(std::string module, std::optional<std::string> alias,
               SourceLocation location)
        : Stmt(ASTNodeKind::ImportStmt, location),
          module(std::move(module)), alias(std::move(alias)) {}
    std::string module;
    std::optional<std::string> alias;
};

} // namespace swan
