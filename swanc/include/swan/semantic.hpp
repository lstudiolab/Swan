#pragma once
#include "swan/ast.hpp"
#include <string>
#include <vector>
namespace swan {
class SemanticAnalyzer {
public:
  bool analyze(const Module& module);
  const std::vector<std::string>& errors() const { return errors_; }
private:
  std::vector<std::string> errors_;
};
}
