#ifndef SWAN_CODEGEN_H
#define SWAN_CODEGEN_H
#include "swan/target.h"
#include "swan/codegen_x86_64.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include "swan/codegen_x86_64_ir.h"
#ifndef SWAN_SIL_BOOTSTRAP
#include "swan/sil_bridge.h"
#endif


static inline bool swan_compile_ir_program(Program *program,SwanIRModule *module,SwanError *error){
#ifndef SWAN_SIL_BOOTSTRAP
    if(swan_sil_try_irgen_program(program,module))return true;
#endif
    return swan_irgen_program(program,module,error);
}

/* SVM is the shared compile engine: every machine target starts from the
   same verified, optimized, target-independent IR. Target backends only lower
   that IR; they never interpret it at runtime. */
static inline bool swan_svm_prepare_ir(Program *program,SwanIRModule *module,
                                       SwanError *error,char *detail,size_t cap){
    uint64_t stage=swan_profile_start();
    bool lowered=swan_compile_ir_program(program,module,error);
    swan_profile_end(SWAN_PROFILE_IRGEN,stage);
    if(!lowered)return false;
    if(detail&&cap)detail[0]=0;
    stage=swan_profile_start();
    bool optimized=swan_ir_optimize(module,detail,cap);
    swan_profile_end(SWAN_PROFILE_IR_OPT,stage);
    if(!optimized){
        if(error&&!error->message[0])swan_error(error,NULL,"%s",detail&&detail[0]?detail:"SVM optimization failed");
        return false;
    }
    char verify[256]={0};
    if(!swan_ir_verify(module,verify,sizeof(verify))){
        if(error)swan_error(error,NULL,"SVM produced invalid optimized IR: %s",verify);
        if(detail&&cap)snprintf(detail,cap,"%s",verify);
        return false;
    }
    return true;
}

/* Prefer verified SIL -> target-independent IR for its supported source subset.
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
        bool optimized=swan_svm_prepare_ir(program,&module,&ir_error,detail,sizeof(detail));
        uint64_t stage=swan_profile_start();bool emitted=optimized&&swan_codegen_x86_64_ir(&module,&ir_output,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_CODEGEN,stage);
        swan_ir_destroy(&module);
        if(emitted){*output=ir_output;return true;}
        free(ir_output.data);
    }
    if(error)memset(error,0,sizeof(*error));
    uint64_t stage=swan_profile_start();bool generated=swan_codegen_x86_64(program, output, error);swan_profile_end(SWAN_PROFILE_AST_CODEGEN,stage);return generated;
}
#endif
