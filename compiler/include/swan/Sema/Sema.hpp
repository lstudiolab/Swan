#pragma once
#include "swan/AST/AST.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include "swan/Sema/TypeRules.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace swan {
class Sema {
public:
    explicit Sema(const Program& program);
    void analyze();
private:
    using Scope = std::unordered_map<std::string,std::pair<std::string,bool>>;
    void analyzeFunction(const StmtPtr& function);
    void analyzeStatement(const StmtPtr& statement);
    void analyzeBlock(const std::vector<StmtPtr>& statements);
    std::string analyzeExpression(const ExprPtr& expression);
    std::pair<std::string,bool>* lookup(const std::string& name);
    static bool numeric(const std::string& type);
    static bool compatible(const std::string& expected,const std::string& actual);
    const Program& program_;
    std::unordered_set<std::string> types_{"int","float","bool","string","void"};
    std::unordered_map<std::string,StmtPtr> functions_;
    std::vector<Scope> scopes_;
    std::string return_type_;
    unsigned loop_depth_ = 0;
};
}

#include "swan/Sema/ScopeAnalysis.hpp"
#include "swan/Sema/DeclarationAnalysis.hpp"
#include "swan/Sema/StatementAnalysis.hpp"
#include "swan/Sema/ExpressionAnalysis.hpp"
