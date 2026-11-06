#pragma once

#include "Swan/AST/Node.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace swan {

struct FunctionDecl final : Stmt {
    FunctionDecl(std::string name, std::vector<std::string> parameters,
                 std::optional<std::string> returnType, std::vector<StmtPtr> body,
                 bool isAsync, SourceLocation location)
        : Stmt(ASTNodeKind::FunctionDecl, location),
          name(std::move(name)), parameters(std::move(parameters)),
          returnType(std::move(returnType)), body(std::move(body)), isAsync(isAsync) {}

    std::string name;
    std::vector<std::string> parameters;
    std::optional<std::string> returnType;
    std::vector<StmtPtr> body;
    bool isAsync;
};

struct StructDecl final : Stmt {
    StructDecl(std::string name, std::vector<StmtPtr> members, SourceLocation location)
        : Stmt(ASTNodeKind::StructDecl, location),
          name(std::move(name)), members(std::move(members)) {}

    std::string name;
    std::vector<StmtPtr> members;
};

} // namespace swan
