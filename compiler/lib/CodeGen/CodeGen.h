#ifndef SWAN_CODEGEN_H
#define SWAN_CODEGEN_H
#include "swan/target.h"
#include "swan/codegen_x86_64.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include "swan/codegen_x86_64_ir.h"
#include "swan/codegen_aarch64.h"
#include "swan/codegen_armv7.h"
#include "swan/codegen_riscv64.h"
#include "swan/codegen_wasm32.h"
#include "swan/codegen_llvm.h"
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

typedef enum {
    SWAN_SVM_OUTPUT_ASSEMBLY,
    SWAN_SVM_OUTPUT_WASM,
    SWAN_SVM_OUTPUT_LLVM_IR
} SwanSVMOutputKind;

typedef struct {
    SwanSVMOutputKind kind;
    char *data;
    size_t length;
} SwanSVMOutput;

static inline void swan_svm_output_destroy(SwanSVMOutput *output) {
    if(!output)return;
    free(output->data);
    memset(output,0,sizeof(*output));
}

/* SVM owns the target-independent pipeline and dispatches to one target
   backend. Each backend emits native assembly or its target module format. */
static inline bool swan_svm_codegen(Program *program,const SwanTarget *target,
                                    SwanSVMOutput *output,SwanError *error) {
    if(!program||!target||!output){
        if(error)swan_error(error,NULL,"SVM requires a program, target, and output buffer");
        return false;
    }
    memset(output,0,sizeof(*output));
    char detail[256]={0};
    Text text={0};
    bool ok=false;
    if(target->architecture==SWAN_ARCH_X86_64){
        /* External C ABI declarations retain Swan's ABI-compatible assembly
           path until the LLVM bridge's symbol and call lowering is fully parity-tested. */
        bool has_external=false;
        for(size_t i=0;i<program->count;i++)
            if(program->functions[i]&&program->functions[i]->external){has_external=true;break;}
        /* Prefer the optimizing LLVM machine backend for closed typed-IR
           programs. LLVM performs instruction selection, register allocation,
           scheduling, and target-specific optimization before assembly output. */
        if(program->import_count==0&&!has_external){
            SwanIRModule module={0};Text llvm={0};
            bool prepared=swan_svm_prepare_ir(program,&module,error,detail,sizeof(detail));
            uint64_t codegen_stage=swan_profile_start();
            bool emitted=prepared&&swan_codegen_llvm_ir(&module,&llvm,target->triple,detail,sizeof(detail));
            swan_profile_end(SWAN_PROFILE_CODEGEN,codegen_stage);
            swan_ir_destroy(&module);
            if(emitted){
                output->kind=SWAN_SVM_OUTPUT_LLVM_IR;
                output->data=llvm.data;output->length=llvm.len;
                return true;
            }
            free(llvm.data);
            if(error)memset(error,0,sizeof(*error));
        }
        /* Keep the native compatibility backend for currently supported AST
           features not yet expressible in typed IR (arrays, logging, imports). */
        ok=swan_codegen(program,&text,target,error);
        output->kind=SWAN_SVM_OUTPUT_ASSEMBLY;
    }else{
        SwanIRModule module={0};
        ok=swan_svm_prepare_ir(program,&module,error,detail,sizeof(detail));
        uint64_t stage=swan_profile_start();
        if(ok)switch(target->architecture){
            case SWAN_ARCH_AARCH64:
                ok=swan_codegen_aarch64_ir(&module,&text,detail,sizeof(detail));
                output->kind=SWAN_SVM_OUTPUT_ASSEMBLY;break;
            case SWAN_ARCH_ARMV7:
                ok=swan_codegen_armv7_ir(&module,&text,detail,sizeof(detail));
                output->kind=SWAN_SVM_OUTPUT_ASSEMBLY;break;
            case SWAN_ARCH_RISCV64:
                ok=swan_codegen_riscv64_ir(&module,&text,detail,sizeof(detail));
                output->kind=SWAN_SVM_OUTPUT_ASSEMBLY;break;
            case SWAN_ARCH_WASM32:
            case SWAN_ARCH_WASM64:{
                SwanWasmBuffer wasm={0};
                ok=target->architecture==SWAN_ARCH_WASM64
                    ?swan_codegen_wasm64(&module,&wasm,detail,sizeof(detail))
                    :swan_codegen_wasm32(&module,&wasm,detail,sizeof(detail));
                if(ok){output->kind=SWAN_SVM_OUTPUT_WASM;output->data=(char*)wasm.data;output->length=wasm.length;}
                else free(wasm.data);
                break;
            }
            case SWAN_ARCH_LLVM:
                ok=swan_codegen_llvm_ir(&module,&text,target->triple,detail,sizeof(detail));
                output->kind=SWAN_SVM_OUTPUT_LLVM_IR;break;
            default:
                ok=false;
                snprintf(detail,sizeof(detail),"SVM has no backend registered for target '%s'",target->name?target->name:"unknown");
                break;
        }
        swan_profile_end(SWAN_PROFILE_CODEGEN,stage);
        swan_ir_destroy(&module);
    }
    if(!ok){
        free(text.data);
        if(error&&!error->message[0])swan_error(error,NULL,"%s",detail[0]?detail:"SVM target compilation failed");
        return false;
    }
    if(output->kind!=SWAN_SVM_OUTPUT_WASM){
        output->data=text.data;
        output->length=text.len;
    }else free(text.data);
    return true;
}

#endif
