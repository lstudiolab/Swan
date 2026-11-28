#pragma once
#include "swan/ast.hpp"
#include <string>
#include <vector>
namespace swan {
class Parser {
public:
  explicit Parser(std::vector<Token> tokens):tokens_(std::move(tokens)){}
  Module parse();
  const std::vector<std::string>& errors() const { return errors_; }
private:
  const Token& peek(std::size_t n=0) const;
  bool match(TokenKind kind);
  bool expect(TokenKind kind, const char* message);
  void synchronize();
  FunctionDecl function();
  std::unique_ptr<BlockStmt> block();
  std::unique_ptr<Stmt> statement();
  std::unique_ptr<Expr> expression(int minPrecedence=0);
  std::unique_ptr<Expr> primary();
  std::string typeName();
  std::vector<Token> tokens_;
  std::size_t pos_=0;
  std::vector<std::string> errors_;
};
}
