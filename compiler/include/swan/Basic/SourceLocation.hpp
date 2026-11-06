#pragma once
#include <cstddef>
namespace swan {
struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
};
}
