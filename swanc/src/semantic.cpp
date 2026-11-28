#include "swan/semantic.hpp"
#include <set>
#include <unordered_set>
namespace swan {
bool SemanticAnalyzer::analyze(const Module& module) {
  errors_.clear();
  std::unordered_set<std::string> functions;
  const std::set<std::string> builtins={"void","int","bool","i8","i16","i32","i64","u8","u16","u32","u64","varint","f32","f64"};
  for (const auto& fn : module.functions) {
    if (!functions.insert(fn.name).second)
      errors_.push_back("duplicate function declaration: "+fn.name);
    if (!builtins.count(fn.returnType))
      errors_.push_back("unknown return type '"+fn.returnType+"' in function "+fn.name);
    std::unordered_set<std::string> params;
    for (const auto& p : fn.params) {
      if (!params.insert(p.first).second)
        errors_.push_back("duplicate parameter '"+p.first+"' in function "+fn.name);
      if (!builtins.count(p.second))
        errors_.push_back("unknown parameter type '"+p.second+"' in function "+fn.name);
    }
  }
  return errors_.empty();
}
}
