#ifndef SWAN_CODEGEN_H
#define SWAN_CODEGEN_H
#include "swan/target.h"
#include "swan/codegen_x86_64.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include "swan/codegen_x86_64_ir.h"

/* Prefer verified target-independent IR for its supported source subset.
   Keep the established AST backend as a compatibility path for frontend
   features not yet represented in IR (arrays, logging and lazy imports). */
static inline bool swan_codegen(Program *program, Text *output,
                                const SwanTarget *target, SwanError *error) {
    if (!target || target->architecture != SWAN_ARCH_X86_64) {
        swan_error(error, NULL, "requested code-generation backend is not implemented");
        return false;
    }
    if(program->import_count==0){
        SwanIRModule module={0};Text ir_output={0};SwanError ir_error={0};char detail[256]={0};
        bool lowered=swan_irgen_program(program,&module,&ir_error);
        bool optimized=lowered&&swan_ir_optimize(&module,detail,sizeof(detail));
        bool emitted=optimized&&swan_codegen_x86_64_ir(&module,&ir_output,detail,sizeof(detail));
        swan_ir_destroy(&module);
        if(emitted){*output=ir_output;return true;}
        free(ir_output.data);
    }
    if(error)memset(error,0,sizeof(*error));
    return swan_codegen_x86_64(program, output, error);
}
#endif
