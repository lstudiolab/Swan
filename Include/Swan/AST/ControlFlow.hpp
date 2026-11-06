#pragma once

#include "Swan/AST/Node.hpp"

#include <string>
#include <utility>
#include <vector>

namespace swan {

struct IfStmt final : Stmt {
    IfStmt(ExprPtr condition, std::vector<StmtPtr> body,
           std::vector<StmtPtr> elseBody, SourceLocation location)
        : Stmt(ASTNodeKind::IfStmt, location),
          condition(std::move(condition)), body(std::move(body)),
          elseBody(std::move(elseBody)) {}

    ExprPtr condition;
    std::vector<StmtPtr> body;
    std::vector<StmtPtr> elseBody;
};

struct WhileStmt final : Stmt {
    WhileStmt(ExprPtr condition, std::vector<StmtPtr> body, SourceLocation location)
        : Stmt(ASTNodeKind::WhileStmt, location),
          condition(std::move(condition)), body(std::move(body)) {}

    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct ForStmt final : Stmt {
    ForStmt(std::string variable, ExprPtr sequence, std::vector<StmtPtr> body,
            SourceLocation location)
        : Stmt(ASTNodeKind::ForStmt, location),
          variable(std::move(variable)), sequence(std::move(sequence)),
          body(std::move(body)) {}

    std::string variable;
    ExprPtr sequence;
    std::vector<StmtPtr> body;
};

} // namespace swan
