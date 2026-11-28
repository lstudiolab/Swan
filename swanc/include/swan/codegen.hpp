#pragma once
#include "swan/ast.hpp"
#include <string>
namespace swan {
class CEmitter {
public:
  std::string emit(const Module& module) const;
private:
  std::string emitStmt(const Stmt& stmt, int indent) const;
  std::string emitExpr(const Expr& expr) const;
};
const char* cType(const std::string& swanType);
}
