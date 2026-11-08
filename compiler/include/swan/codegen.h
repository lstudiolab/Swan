#ifndef SWAN_CODEGEN_H
#define SWAN_CODEGEN_H
#include "swan/target.h"
#include "swan/codegen_x86_64.h"

/* Target-dispatch boundary. A target is accepted here only when a real
   backend exists; planned targets are rejected by the driver. */
static inline bool swan_codegen(Program *program, Text *output,
                                const SwanTarget *target, SwanError *error) {
    if (!target || target->architecture != SWAN_ARCH_X86_64) {
        swan_error(error, NULL, "requested code-generation backend is not implemented");
        return false;
    }
    return swan_codegen_x86_64(program, output, error);
}
#endif
