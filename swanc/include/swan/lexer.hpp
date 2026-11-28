#pragma once
#include "swan/token.hpp"
#include <string>
#include <vector>
namespace swan {
class Lexer {
public:
  explicit Lexer(const std::string& source) : source_(source) {}
  std::vector<Token> scan();
  const std::vector<std::string>& errors() const { return errors_; }
private:
  char peek(std::size_t look=0) const;
  char take();
  void skipTrivia();
  Token make(TokenKind kind, std::size_t start, SourceLocation loc) const;
  const std::string& source_;
  std::size_t pos_=0, line_=1, column_=1;
  std::vector<std::string> errors_;
};
}
