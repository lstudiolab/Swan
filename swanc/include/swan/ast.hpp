#pragma once
#include "swan/token.hpp"
#include <memory>
#include <string>
#include <vector>
namespace swan {
struct Expr { virtual ~Expr()=default; };
struct LiteralExpr : Expr { std::string value; explicit LiteralExpr(std::string v):value(std::move(v)){} };
struct NameExpr : Expr { std::string name; explicit NameExpr(std::string n):name(std::move(n)){} };
struct UnaryExpr : Expr { TokenKind op; std::unique_ptr<Expr> rhs; };
struct BinaryExpr : Expr { std::unique_ptr<Expr> lhs, rhs; TokenKind op; };
struct Stmt { virtual ~Stmt()=default; };
struct BlockStmt : Stmt { std::vector<std::unique_ptr<Stmt>> statements; };
struct ReturnStmt : Stmt { std::unique_ptr<Expr> value; };
struct VarStmt : Stmt { std::string name, type; bool mutableValue=false; std::unique_ptr<Expr> value; };
struct ExprStmt : Stmt { std::unique_ptr<Expr> value; };
struct IfStmt : Stmt { std::unique_ptr<Expr> condition; std::unique_ptr<Stmt> thenBranch, elseBranch; };
struct WhileStmt : Stmt { std::unique_ptr<Expr> condition; std::unique_ptr<Stmt> body; };
struct FunctionDecl { std::string name, returnType="void"; std::vector<std::pair<std::string,std::string>> params; std::unique_ptr<BlockStmt> body; bool isExtern=false; };
struct Module { std::vector<FunctionDecl> functions; };
}
