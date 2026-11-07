#pragma once
#include "swan/Basic/Token.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace swan {
struct Expr;
using ExprPtr = std::shared_ptr<Expr>;
struct Expr {
    enum class Kind { Literal, Name, Unary, Binary, Call, Member };
    enum class ResolvedType : std::uint8_t { Unknown, Int, Float, Bool, String, None, Void };
    Kind kind = Kind::Literal;
    ResolvedType resolved_type = ResolvedType::Unknown;
    Token token;
    std::string value;
    std::string op;
    std::string literal_kind;
    ExprPtr left;
    ExprPtr right;
    std::vector<ExprPtr> arguments;
};
}
