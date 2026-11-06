#pragma once

#include "Swan/AST/Node.hpp"

#include <vector>

namespace swan {

struct Module {
    std::vector<StmtPtr> statements;
};

} // namespace swan
