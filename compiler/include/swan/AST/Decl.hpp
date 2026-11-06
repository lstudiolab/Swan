#pragma once
#include "swan/Basic/Token.hpp"
#include <string>
namespace swan {
struct Param { std::string name; std::string type; Token token; };
struct Field { std::string name; std::string type; Token token; };
}
