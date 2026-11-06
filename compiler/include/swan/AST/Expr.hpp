#pragma once
#include "swan/Basic/Token.hpp"
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
}
