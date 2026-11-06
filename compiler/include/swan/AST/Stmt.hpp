#pragma once
#include "swan/AST/Expr.hpp"
#include "swan/AST/Decl.hpp"
#include <memory>
#include <string>
#include <vector>
namespace swan {
struct Stmt;
using StmtPtr = std::shared_ptr<Stmt>;
struct Stmt {
    enum class Kind {
        Function, Struct, Variable, If, While, Return, Break,
        Continue, Pass, Log, Expression, Import
    };
    Kind kind = Kind::Pass;
    Token token;
    std::string name;
    std::string type;
    std::string module;
    std::string alias;
    std::string return_type = "void";
    bool mutable_value = false;
    std::vector<Param> parameters;
    std::vector<Field> fields;
    std::vector<StmtPtr> body;
    std::vector<StmtPtr> alternate;
    ExprPtr value;
    ExprPtr condition;
};
}
