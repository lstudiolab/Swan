#pragma once
#include "swan/Token.hpp"
#include <memory>
#include <string>
#include <vector>

namespace swan {
struct Expr;
using ExprPtr = std::shared_ptr<Expr>;
struct Expr {
    enum class Kind { Literal, Name, Unary, Binary, Call, Member };
    Kind kind = Kind::Literal;
    Token token;
    std::string value;
    std::string op;
    std::string literal_kind;
    ExprPtr left;
    ExprPtr right;
    std::vector<ExprPtr> arguments;
};

struct Param { std::string name; std::string type; Token token; };
struct Field { std::string name; std::string type; Token token; };

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
using Program = std::vector<StmtPtr>;
}
