#pragma once
#include "swan/AST/AST.hpp"
#include "swan/CodeGen/CType.hpp"
#include "swan/CodeGen/StringLiteral.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace swan {
class CGenerator {
public:
    explicit CGenerator(const Program& program);
    std::string generate();
private:
    std::string cType(const std::string& type) const;
    std::string expression(const ExprPtr& expression) const;
    std::string inferredType(const ExprPtr& expression) const;
    std::string quote(const std::string& value) const;
    void line(const std::string& value = {});
    void function(const StmtPtr& function);
    void block(const std::vector<StmtPtr>& statements);
    void statement(const StmtPtr& statement);
    const Program& program_;
    std::string output_;
    unsigned indent_ = 0;
    std::unordered_set<std::string> structs_;
    std::unordered_map<std::string,StmtPtr> functions_;
    std::unordered_map<std::string,std::string> symbols_;
    std::vector<std::unordered_map<std::string,std::string>> scopes_;
};
}

#include "swan/CodeGen/ProgramEmitter.hpp"
#include "swan/CodeGen/ExpressionEmitter.hpp"
#include "swan/CodeGen/StatementEmitter.hpp"
