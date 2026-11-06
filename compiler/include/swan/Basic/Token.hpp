#pragma once
#include "swan/Basic/SourceLocation.hpp"
#include <cstddef>
#include <string>
namespace swan {
struct Token {
    std::string kind;
    std::string value;
    std::size_t line = 1;
    std::size_t column = 1;
    SourceLocation location() const noexcept { return {line, column}; }
};
}
