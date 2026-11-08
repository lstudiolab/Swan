#ifndef SWAN_OPTIMIZATION_H
#define SWAN_OPTIMIZATION_H
#include "swan/ir.h"

/* Target-independent, semantics-preserving scalar IR optimizations. */
static inline bool swan_ir_constant(const SwanIRFunction *fn, uint32_t id,
                                    int64_t *value) {
    if (!id || id <= fn->parameter_count) return false;
    for (size_t b=0; b<fn->block_count; ++b)
        for (size_t i=0; i<fn->blocks[b].instruction_count; ++i) {
            const SwanIRInst *in=&fn->blocks[b].instructions[i];
            if (in->result==id && in->op==SWAN_IR_CONST) { *value=in->immediate; return true; }
            if (in->result==id) return false;
        }
    return false;
}
static inline bool swan_ir_fold_instruction(SwanIRFunction *fn, SwanIRInst *in) {
    if (in->op==SWAN_IR_CONST || in->op==SWAN_IR_PHI ||
        in->op==SWAN_IR_CALL || in->op==SWAN_IR_ALLOCA ||
        in->op==SWAN_IR_LOAD || in->op==SWAN_IR_STORE ||
        in->op==SWAN_IR_SELECT) return false;
    int64_t a,b;
    if (!swan_ir_constant(fn,in->a,&a) || !swan_ir_constant(fn,in->b,&b)) return false;
    int64_t value=0; bool fold=true;
    switch(in->op) {
        case SWAN_IR_ADD: value=(int64_t)((uint64_t)a+(uint64_t)b); break;
        case SWAN_IR_SUB: value=(int64_t)((uint64_t)a-(uint64_t)b); break;
        case SWAN_IR_MUL: value=(int64_t)((uint64_t)a*(uint64_t)b); break;
        case SWAN_IR_SDIV: if(!b||(a==INT64_MIN&&b==-1)) fold=false; else value=a/b; break;
        case SWAN_IR_UDIV: if(!b) fold=false; else value=(int64_t)((uint64_t)a/(uint64_t)b); break;
        case SWAN_IR_SREM: if(!b||(a==INT64_MIN&&b==-1)) fold=false; else value=a%b; break;
        case SWAN_IR_UREM: if(!b) fold=false; else value=(int64_t)((uint64_t)a%(uint64_t)b); break;
        case SWAN_IR_AND: value=a&b; break; case SWAN_IR_OR: value=a|b; break;
        case SWAN_IR_XOR: value=a^b; break;
        case SWAN_IR_SHL: value=(int64_t)((uint64_t)a<<((uint64_t)b&63u)); break;
        case SWAN_IR_LSHR: value=(int64_t)((uint64_t)a>>((uint64_t)b&63u)); break;
        case SWAN_IR_ASHR: { unsigned shift=(unsigned)((uint64_t)b&63u);uint64_t bits=(uint64_t)a;value=(int64_t)(shift?((bits>>shift)|((bits>>63)?(~UINT64_C(0)<<(64u-shift)):0)):bits);break;}
        case SWAN_IR_ICMP_EQ: value=a==b; break; case SWAN_IR_ICMP_NE: value=a!=b; break;
        case SWAN_IR_ICMP_SLT: value=a<b; break; case SWAN_IR_ICMP_SLE: value=a<=b; break;
        case SWAN_IR_ICMP_SGT: value=a>b; break; case SWAN_IR_ICMP_SGE: value=a>=b; break;
        default: fold=false; break;
    }
    if(!fold)return false;
    in->op=SWAN_IR_CONST;in->a=in->b=in->c=0;in->immediate=value;
    return true;
}
static inline size_t swan_ir_use_count(const SwanIRFunction *fn,uint32_t value){
    size_t uses=0;
    for(size_t b=0;b<fn->block_count;b++){
        const SwanIRBlock *bb=&fn->blocks[b];
        for(size_t i=0;i<bb->instruction_count;i++){
            const SwanIRInst *in=&bb->instructions[i];
            uses+=(in->a==value)+(in->b==value)+(in->c==value);
            for(size_t k=0;k<in->incoming_count;k++)uses+=in->incoming[k].value==value;
        }
        uses+=bb->terminator.value==value;
    }
    return uses;
}
static inline bool swan_ir_inst_has_side_effect(const SwanIRInst *in){
    return in->op==SWAN_IR_CALL||in->op==SWAN_IR_STORE||in->op==SWAN_IR_LOAD;
}
static inline bool swan_ir_dead_code_eliminate(SwanIRModule *module){
    bool changed=false,again=true;
    while(again){
        again=false;
        for(size_t f=0;f<module->function_count;f++){
            SwanIRFunction *fn=&module->functions[f];
            for(size_t b=0;b<fn->block_count;b++){
                SwanIRBlock *bb=&fn->blocks[b];size_t out=0;
                for(size_t i=0;i<bb->instruction_count;i++){
                    SwanIRInst in=bb->instructions[i];
                    bool dead=in.result&&!swan_ir_inst_has_side_effect(&in)&&swan_ir_use_count(fn,in.result)==0;
                    if(dead){free(in.incoming);again=true;changed=true;continue;}
                    bb->instructions[out++]=in;
                }
                bb->instruction_count=out;
            }
        }
    }
    return changed;
}
typedef enum { SWAN_IR_PASS_CONSTANT_FOLD, SWAN_IR_PASS_DEAD_CODE } SwanIRPassKind;
typedef struct { SwanIRPassKind kind; const char *name; } SwanIRPass;
static const SwanIRPass swan_ir_default_passes[]={
    {SWAN_IR_PASS_CONSTANT_FOLD,"constant-folding"},
    {SWAN_IR_PASS_DEAD_CODE,"dead-code-elimination"}
};
static inline bool swan_ir_run_pass(SwanIRModule *module,SwanIRPassKind pass,char *error,size_t cap){
    if(pass==SWAN_IR_PASS_CONSTANT_FOLD){
        for(size_t f=0;f<module->function_count;f++)
            for(size_t b=0;b<module->functions[f].block_count;b++)
                for(size_t i=0;i<module->functions[f].blocks[b].instruction_count;i++)
                    (void)swan_ir_fold_instruction(&module->functions[f],&module->functions[f].blocks[b].instructions[i]);
        return swan_ir_verify(module,error,cap);
    }
    if(pass==SWAN_IR_PASS_DEAD_CODE){(void)swan_ir_dead_code_eliminate(module);return swan_ir_verify(module,error,cap);}
    if(error&&cap)snprintf(error,cap,"unknown IR optimization pass");return false;
}
static inline bool swan_ir_optimize(SwanIRModule *module, char *error, size_t cap) {
    if(!swan_ir_verify(module,error,cap))return false;
    for(size_t i=0;i<sizeof(swan_ir_default_passes)/sizeof(swan_ir_default_passes[0]);i++)
        if(!swan_ir_run_pass(module,swan_ir_default_passes[i].kind,error,cap))return false;
    return true;
}
#endif
