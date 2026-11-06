#pragma once

#include "Swan/AST/AST.hpp"
#include "Swan/Sema/Diagnostic.hpp"
#include "Swan/Sema/Scope.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace swan {

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(const Module& module);

    bool analyze();
    const std::vector<SemaDiagnostic>& diagnostics() const noexcept;
    const Scope& globalScope() const noexcept;

private:
    const Module& module_;
    Scope globalScope_;
    Scope* currentScope_ = nullptr;
    std::vector<SemaDiagnostic> diagnostics_;
    Type expectedReturnType_;
    bool hasExpectedReturnType_ = false;
    bool insideFunction_ = false;
    std::size_t loopDepth_ = 0;

    void report(SourceLocation location, std::string code, std::string message);
    void predeclare();
    void analyzeStatement(const Stmt& statement);
    Type analyzeExpression(const Expr& expression);
    Type resolveType(const std::string& name, SourceLocation location);
    bool compatible(const Type& expected, const Type& actual) const noexcept;
    void analyzeBlock(const std::vector<StmtPtr>& statements);
    void analyzeFunction(const FunctionDecl& function);
    void analyzeStruct(const StructDecl& structure);
    void analyzeVariable(const VarDecl& variable, bool field = false);
    void analyzeIf(const IfStmt& statement);
    void analyzeWhile(const WhileStmt& statement);
    void analyzeFor(const ForStmt& statement);
};

} // namespace swan
