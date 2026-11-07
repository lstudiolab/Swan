#pragma once
#include "swan/Basic/SourceLocation.hpp"
#include <string>
namespace swan {
struct Param { std::string name; std::string type; SourceLocation token; };
struct Field { std::string name; std::string type; SourceLocation token; };
}
