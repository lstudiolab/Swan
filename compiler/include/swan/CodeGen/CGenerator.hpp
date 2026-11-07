#pragma once
#include "swan/AST/AST.hpp"
#include "swan/CodeGen/CType.hpp"
#include "swan/CodeGen/StringLiteral.hpp"
#include <string>
#include <string_view>
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
    std::unordered_set<std::string_view> structs_;
};
}
#include "swan/CodeGen/ProgramEmitter.hpp"
#include "swan/CodeGen/ExpressionEmitter.hpp"
#include "swan/CodeGen/StatementEmitter.hpp"
