#pragma once
#include "swan/AST/AST.hpp"
#include "swan/CodeGen/CType.hpp"
#include "swan/CodeGen/StringLiteral.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace swan {
class CGenerator {
public:
    explicit CGenerator(const Program& program);
    std::string generate();
private:
    std::string cType(const std::string& type) const;
    void appendCType(std::string& output, std::string_view type) const;
    std::string expression(const ExprPtr& expression) const;
    void appendExpression(std::string& output, const ExprPtr& expression) const;
    std::string_view inferredType(const ExprPtr& expression) const;
    std::string quote(const std::string& value) const;
    void line(const std::string& value = {});
    void beginLine();
    void endLine();
    void function(const StmtPtr& function);
    void block(const std::vector<StmtPtr>& statements);
    void statement(const StmtPtr& statement);
    const Program& program_;
    std::string output_;
    unsigned indent_ = 0;
    StructureSet structs_;
    StringViewMap<const Stmt*> functions_;
    StringViewMap<std::string> symbols_;
    std::vector<StringViewMap<std::string>> scopes_;
};
}

#include "swan/CodeGen/ProgramEmitter.hpp"
#include "swan/CodeGen/ExpressionEmitter.hpp"
#include "swan/CodeGen/StatementEmitter.hpp"
