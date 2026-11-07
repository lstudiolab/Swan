#pragma once
#include "swan/Basic/Token.hpp"
#include <memory>
#include <string>
#include <vector>
namespace swan {
struct Expr;
using ExprPtr = std::unique_ptr<Expr>;
struct Expr {
    enum class Kind { Literal, Name, Unary, Binary, Call, Member, Index };
    Kind kind = Kind::Literal;
    Token token;
    std::string resolved_type;
    ExprPtr left;
    ExprPtr right;
    std::vector<ExprPtr> arguments;
};
}
