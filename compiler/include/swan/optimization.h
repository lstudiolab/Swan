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
        case SWAN_IR_ASHR: value=a>>((uint64_t)b&63u); break;
        case SWAN_IR_ICMP_EQ: value=a==b; break; case SWAN_IR_ICMP_NE: value=a!=b; break;
        case SWAN_IR_ICMP_SLT: value=a<b; break; case SWAN_IR_ICMP_SLE: value=a<=b; break;
        case SWAN_IR_ICMP_SGT: value=a>b; break; case SWAN_IR_ICMP_SGE: value=a>=b; break;
        default: fold=false; break;
    }
    if(!fold)return false;
    in->op=SWAN_IR_CONST;in->a=in->b=in->c=0;in->immediate=value;
    return true;
}
static inline bool swan_ir_optimize(SwanIRModule *module, char *error, size_t cap) {
    if(!swan_ir_verify(module,error,cap))return false;
    for(size_t f=0;f<module->function_count;++f)
        for(size_t b=0;b<module->functions[f].block_count;++b)
            for(size_t i=0;i<module->functions[f].blocks[b].instruction_count;++i)
                (void)swan_ir_fold_instruction(&module->functions[f],
                    &module->functions[f].blocks[b].instructions[i]);
    return swan_ir_verify(module,error,cap);
}
#endif
