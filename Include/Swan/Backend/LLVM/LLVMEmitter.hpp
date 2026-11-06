#pragma once

#include "Swan/SIL/SIL.hpp"

#include <string>
#include <vector>

namespace swan::backend::llvm {

struct EmitResult {
    std::string output;
    std::vector<SemaDiagnostic> diagnostics;
    bool ok() const noexcept { return diagnostics.empty(); }
};

class LLVMEmitter {
public:
    EmitResult emit(const sil::Module& module) const;
};

} // namespace swan::backend::llvm
