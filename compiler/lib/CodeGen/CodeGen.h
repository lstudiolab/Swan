#ifndef BREW_CODEGEN_H
#define BREW_CODEGEN_H
#include "brew/target.h"
#include "brew/codegen_x86_64.h"
#include "brew/irgen.h"
#include "../SIL/SIL.h"
#include "brew/optimization.h"
#include "brew/codegen_x86_64_ir.h"
#include "brew/codegen_aarch64.h"
#include "brew/codegen_wasm64.h"


static inline bool brew_compile_ir_program(Program *program,BrewIRModule *module,BrewError *error){
    return brew_irgen_program(program,module,error);
}

/* SVM is the shared compile engine: every machine target starts from the
   same verified, optimized, target-independent IR. Target backends only lower
   that IR; they never interpret it at runtime. */
static inline bool brew_svm_prepare_ir(Program *program,BrewIRModule *module,
                                       BrewError *error,char *detail,size_t cap){
    /* Swift-style SIL is a mandatory checked and optimized high-level IR stage. */
    uint64_t stage=brew_profile_start();
    bool sil_ready=brew_sil_prepare_program(program,error);
    brew_profile_end(BREW_PROFILE_SIL,stage);
    if(!sil_ready)return false;
    stage=brew_profile_start();
    bool lowered=brew_compile_ir_program(program,module,error);
    brew_profile_end(BREW_PROFILE_IRGEN,stage);
    if(!lowered)return false;
    if(detail&&cap)detail[0]=0;
    stage=brew_profile_start();
    bool optimized=brew_ir_optimize(module,detail,cap);
    brew_profile_end(BREW_PROFILE_IR_OPT,stage);
    if(!optimized){
        if(error&&!error->message[0])brew_error(error,NULL,"%s",detail&&detail[0]?detail:"SVM optimization failed");
        return false;
    }
    char verify[256]={0};
    if(!brew_ir_verify(module,verify,sizeof(verify))){
        if(error)brew_error(error,NULL,"SVM produced invalid optimized IR: %s",verify);
        if(detail&&cap)snprintf(detail,cap,"%s",verify);
        return false;
    }
    return true;
}

/* The AST backend is a deliberate compatibility path, not a recovery path.
   Select it only when the source contains a construct that is known to require
   it. An IR verifier or optimizer failure must remain a hard compilation error. */
static inline bool brew_expr_requires_ast_compatibility(const Expr *expression){
    if(!expression)return false;
    if(expression->kind==E_INDEX||expression->kind==E_FLOAT||
       expression->kind==E_STRING||expression->kind==E_MEMBER)return true;
    if(expression->kind==E_CALL&&expression->left&&expression->left->kind==E_NAME){
        const char *name=expression->left->token.text;
        if(name&&(!strcmp(name,"array")||!strcmp(name,"array_u32")||
                  !strcmp(name,"array_unchecked")||!strcmp(name,"array_u32_unchecked")))
            return true;
    }
    if(brew_expr_requires_ast_compatibility(expression->left)||
       brew_expr_requires_ast_compatibility(expression->right))return true;
    for(size_t i=0;i<expression->argc;i++)
        if(brew_expr_requires_ast_compatibility(expression->args[i]))return true;
    return false;
}
static inline bool brew_block_requires_ast_compatibility(Stmt **body,size_t count){
    for(size_t i=0;i<count;i++){
        const Stmt *statement=body[i];
        if(!statement)continue;
        if(statement->kind==S_LOG||
           brew_expr_requires_ast_compatibility(statement->value)||
           brew_expr_requires_ast_compatibility(statement->condition)||
           brew_block_requires_ast_compatibility(statement->body,statement->body_count)||
           brew_block_requires_ast_compatibility(statement->alternate,statement->alt_count))
            return true;
    }
    return false;
}
static inline bool brew_block_exceeds_ir_local_limit(
    Stmt **body,size_t count,size_t active,size_t *maximum){
    for(size_t i=0;i<count;i++){
        const Stmt *statement=body[i];
        if(!statement)continue;
        if(statement->kind==S_VARIABLE){
            if(active==SIZE_MAX)return true;
            active++;
            if(active>*maximum)*maximum=active;
            if(active>256)return true;
        }
        if(statement->kind==S_IF||statement->kind==S_WHILE){
            if(brew_block_exceeds_ir_local_limit(statement->body,
                    statement->body_count,active,maximum)||
               brew_block_exceeds_ir_local_limit(statement->alternate,
                    statement->alt_count,active,maximum))return true;
        }
    }
    return false;
}
static inline bool brew_program_requires_ast_compatibility(const Program *program){
    if(!program||(!program->bundle_imports&&program->import_count))return true;
    for(size_t i=0;i<program->count;i++){
        const Stmt *function=program->functions[i];
        if(!function)continue;
        if(program->codegen_source_index>=0&&
           function->source_index!=(size_t)program->codegen_source_index)continue;
        size_t maximum_locals=0;
        if(brew_block_exceeds_ir_local_limit(function->body,
                function->body_count,0,&maximum_locals)||
           brew_block_requires_ast_compatibility(function->body,
                                                  function->body_count))
            return true;
    }
    return false;
}
static inline bool brew_codegen(Program *program, Text *output,
                                const BrewTarget *target, BrewError *error) {
    if(error)memset(error,0,sizeof(*error));
    if(output)memset(output,0,sizeof(*output));
    if(!program||!output||!target){
        brew_error(error,NULL,"code generation requires a program, output buffer, and target");
        return false;
    }
    if (target->architecture != BREW_ARCH_X86_64) {
        brew_error(error, NULL, "requested code-generation backend is not implemented");
        return false;
    }
    if(!program->no_heap&&!program->codegen_force_ast&&!brew_program_requires_ast_compatibility(program)){
        BrewIRModule module={0};
        Text ir_output={0};
        BrewError ir_error={0};
        char detail[256]={0};
        bool prepared=brew_svm_prepare_ir(program,&module,&ir_error,detail,sizeof(detail));
        if(!prepared){
            if(error)*error=ir_error;
            if(error&&!error->message[0])
                brew_error(error,NULL,"%s",detail[0]?detail:"Brew IR preparation failed");
            brew_ir_destroy(&module);
            return false;
        }
        uint64_t stage=brew_profile_start();
        bool emitted=brew_codegen_x86_64_ir(&module,&ir_output,detail,sizeof(detail));
        brew_profile_end(BREW_PROFILE_CODEGEN,stage);
        brew_ir_destroy(&module);
        if(!emitted){
            free(ir_output.data);
            if(error)brew_error(error,NULL,"%s",detail[0]?detail:"x86-64 IR code generation failed");
            return false;
        }
        *output=ir_output;
        return true;
    }
    /* The AST compatibility backend also passes through SIL; it is not an escape hatch. */
    uint64_t sil_stage=brew_profile_start();
    bool sil_ready=brew_sil_prepare_program(program,error);
    brew_profile_end(BREW_PROFILE_SIL,sil_stage);
    if(!sil_ready)return false;
    uint64_t stage=brew_profile_start();
    bool generated=brew_codegen_x86_64(program,output,error);
    brew_profile_end(BREW_PROFILE_AST_CODEGEN,stage);
    return generated;
}

typedef enum {
    BREW_SVM_OUTPUT_ASSEMBLY,
    BREW_SVM_OUTPUT_WASM
} BrewSVMOutputKind;

typedef struct {
    BrewSVMOutputKind kind;
    char *data;
    size_t length;
} BrewSVMOutput;

static inline void brew_svm_output_destroy(BrewSVMOutput *output) {
    if(!output)return;
    free(output->data);
    memset(output,0,sizeof(*output));
}

/* SVM owns the target-independent pipeline and dispatches to one target
   backend. Each backend emits native assembly or its target module format. */
static inline bool brew_svm_codegen(Program *program,const BrewTarget *target,
                                    BrewSVMOutput *output,BrewError *error) {
    if(error)memset(error,0,sizeof(*error));
    if(output)memset(output,0,sizeof(*output));
    if(!program||!target||!output){
        brew_error(error,NULL,"SVM requires a program, target, and output buffer");
        return false;
    }
    char detail[256]={0};
    Text text={0};
    bool ok=false;
    if(target->architecture==BREW_ARCH_X86_64){
        ok=brew_codegen(program,&text,target,error);
        output->kind=BREW_SVM_OUTPUT_ASSEMBLY;
    }else{
        BrewIRModule module={0};
        ok=brew_svm_prepare_ir(program,&module,error,detail,sizeof(detail));
        uint64_t stage=brew_profile_start();
        if(ok)switch(target->architecture){
            case BREW_ARCH_AARCH64:
                ok=brew_codegen_aarch64_ir(&module,&text,detail,sizeof(detail));
                output->kind=BREW_SVM_OUTPUT_ASSEMBLY;break;
            case BREW_ARCH_WASM64:{
                BrewWasmBuffer wasm={0};
                ok=brew_codegen_wasm64(&module,&wasm,detail,sizeof(detail));
                if(ok){output->kind=BREW_SVM_OUTPUT_WASM;output->data=(char*)wasm.data;output->length=wasm.length;}
                else free(wasm.data);
                break;
            }
            default:
                ok=false;
                snprintf(detail,sizeof(detail),"SVM has no backend registered for target '%s'",target->name?target->name:"unknown");
                break;
        }
        brew_profile_end(BREW_PROFILE_CODEGEN,stage);
        brew_ir_destroy(&module);
    }
    if(!ok){
        free(text.data);
        if(error&&!error->message[0])brew_error(error,NULL,"%s",detail[0]?detail:"SVM target compilation failed");
        return false;
    }
    if(output->kind!=BREW_SVM_OUTPUT_WASM){
        output->data=text.data;
        output->length=text.len;
    }else free(text.data);
    return true;
}

#endif
