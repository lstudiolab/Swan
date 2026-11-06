#pragma once
#include <string>
#include <string_view>

namespace swan {
class Compiler {
public:
    std::string compile(std::string_view source) const;
};
}
