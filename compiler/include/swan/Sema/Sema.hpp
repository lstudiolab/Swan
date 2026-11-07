#pragma once
#include "swan/AST/AST.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include "swan/Sema/TypeRules.hpp"
#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace swan {
class Sema {
public:
    explicit Sema(const Program& program);
    void analyze();
private:
    using Symbol = std::pair<std::string, bool>;
    struct Scope {
        std::unordered_map<std::string, Symbol> declarations;
        std::vector<std::pair<std::string_view, Symbol*>> shadowed;
    };
    struct StringViewHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };
    void analyzeFunction(const StmtPtr& function);
    void analyzeStatement(const StmtPtr& statement);
    void analyzeBlock(const std::vector<StmtPtr>& statements);
    std::string_view analyzeExpression(const ExprPtr& expression);
    bool alwaysReturns(const std::vector<StmtPtr>& statements) const;
    Symbol* lookup(std::string_view name);
    static bool numeric(std::string_view type);
    static bool compatible(std::string_view expected, std::string_view actual);
    const Program& program_;
    std::unordered_set<std::string> types_{"int","float","bool","string","void"};
    std::unordered_map<std::string,const Stmt*> functions_;
    std::vector<Scope> scopes_;
    std::unordered_map<std::string_view, Symbol*, StringViewHash, std::equal_to<>> active_symbols_;
    std::string return_type_;
    unsigned loop_depth_ = 0;
};
}

#include "swan/Sema/ScopeAnalysis.hpp"
#include "swan/Sema/DeclarationAnalysis.hpp"
#include "swan/Sema/StatementAnalysis.hpp"
#include "swan/Sema/ExpressionAnalysis.hpp"
