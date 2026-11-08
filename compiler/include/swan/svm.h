#ifndef SWAN_SVM_H
#define SWAN_SVM_H
#include "swan/ir.h"
#include <limits.h>

/* SVM is Swan's portable, register-based virtual machine. Its instruction
 * operands are SSA value IDs, so the same verified IR is executable on any
 * host CPU without a host-specific instruction selector. */
#define SWAN_SVM_MAGIC "SWANVM01"
#define SWAN_SVM_VERSION 1u
#define SWAN_SVM_MAX_CALL_DEPTH 256u

typedef struct {
    const SwanIRModule *module;
    uint64_t fuel;
    unsigned depth;
    char *error;
    size_t error_capacity;
} SwanVM;

static inline bool swan_svm_fail(SwanVM *vm, const char *message) {
    if (vm->error && vm->error_capacity)
        snprintf(vm->error, vm->error_capacity, "%s", message);
    return false;
}

static inline bool swan_svm_eval(SwanVM *vm, size_t function_index,
                                 const int64_t *arguments, size_t argument_count,
                                 int64_t *result) {
    if (function_index >= vm->module->function_count)
        return swan_svm_fail(vm, "SVM call references an invalid function");
    if (vm->depth >= SWAN_SVM_MAX_CALL_DEPTH)
        return swan_svm_fail(vm, "SVM maximum call depth exceeded");
    const SwanIRFunction *fn = &vm->module->functions[function_index];
    if (argument_count != fn->parameter_count)
        return swan_svm_fail(vm, "SVM function argument count mismatch");
    size_t value_count = (size_t)fn->next_value + 1;
    int64_t *values = calloc(value_count, sizeof(*values));
    if (!values) return swan_svm_fail(vm, "SVM out of memory allocating registers");
    int64_t memory[4096]={0}; size_t memory_count=0;
    for (size_t i = 0; i < argument_count; ++i) values[i + 1] = arguments[i];
    for(size_t bi=0;bi<fn->block_count;bi++)for(size_t ii=0;ii<fn->blocks[bi].instruction_count;ii++){
        const SwanIRInst *alloc=&fn->blocks[bi].instructions[ii];
        if(alloc->op==SWAN_IR_ALLOCA){
            if(memory_count>=4095||alloc->result>=value_count){free(values);return swan_svm_fail(vm,"SVM local memory limit exceeded");}
            values[alloc->result]=(int64_t)++memory_count;
        }
    }
    ++vm->depth;
    size_t block_index = 0, predecessor = SIZE_MAX;
    bool returned = false, ok = true;
    while (ok && !returned) {
        if (block_index >= fn->block_count) { ok = swan_svm_fail(vm, "SVM branch target out of range"); break; }
        const SwanIRBlock *block = &fn->blocks[block_index];
        for (size_t i = 0; i < block->instruction_count && ok; ++i) {
            if (vm->fuel == 0) { ok = swan_svm_fail(vm, "SVM execution fuel exhausted"); break; }
            --vm->fuel;
            const SwanIRInst *in = &block->instructions[i];
            int64_t a = in->a < value_count ? values[in->a] : 0;
            int64_t b = in->b < value_count ? values[in->b] : 0;
            int64_t v = 0;
            switch (in->op) {
                case SWAN_IR_CONST: v = in->immediate; break;
                case SWAN_IR_ADD: v = (int64_t)((uint64_t)a + (uint64_t)b); break;
                case SWAN_IR_SUB: v = (int64_t)((uint64_t)a - (uint64_t)b); break;
                case SWAN_IR_MUL: v = (int64_t)((uint64_t)a * (uint64_t)b); break;
                case SWAN_IR_SDIV:
                    if (!b || (a == INT64_MIN && b == -1)) { ok = swan_svm_fail(vm, "SVM signed division trap"); break; }
                    v = a / b; break;
                case SWAN_IR_UDIV:
                    if (!b) { ok = swan_svm_fail(vm, "SVM unsigned division by zero"); break; }
                    v = (int64_t)((uint64_t)a / (uint64_t)b); break;
                case SWAN_IR_SREM:
                    if (!b || (a == INT64_MIN && b == -1)) { ok = swan_svm_fail(vm, "SVM signed remainder trap"); break; }
                    v = a % b; break;
                case SWAN_IR_UREM:
                    if (!b) { ok = swan_svm_fail(vm, "SVM unsigned remainder by zero"); break; }
                    v = (int64_t)((uint64_t)a % (uint64_t)b); break;
                case SWAN_IR_AND: v = a & b; break;
                case SWAN_IR_OR: v = a | b; break;
                case SWAN_IR_XOR: v = a ^ b; break;
                case SWAN_IR_SHL: v = (int64_t)((uint64_t)a << ((uint64_t)b & 63u)); break;
                case SWAN_IR_LSHR: v = (int64_t)((uint64_t)a >> ((uint64_t)b & 63u)); break;
                case SWAN_IR_ASHR: { unsigned shift=(unsigned)((uint64_t)b & 63u); uint64_t bits=(uint64_t)a; v=(int64_t)(shift ? ((bits >> shift) | ((bits >> 63) ? (~UINT64_C(0) << (64u-shift)) : 0)) : bits); break; }
                case SWAN_IR_ICMP_EQ: v = a == b; break;
                case SWAN_IR_ICMP_NE: v = a != b; break;
                case SWAN_IR_ICMP_SLT: v = a < b; break;
                case SWAN_IR_ICMP_SLE: v = a <= b; break;
                case SWAN_IR_ICMP_SGT: v = a > b; break;
                case SWAN_IR_ICMP_SGE: v = a >= b; break;
                case SWAN_IR_SELECT: v = a ? values[in->b] : values[in->c]; break;
                case SWAN_IR_PHI: {
                    bool found = false;
                    for (size_t k = 0; k < in->incoming_count; ++k)
                        if (in->incoming[k].block == predecessor) {
                            uint32_t id = in->incoming[k].value;
                            if (id >= value_count) { ok = swan_svm_fail(vm, "SVM PHI value out of range"); }
                            else v = values[id];
                            found = true; break;
                        }
                    if (!found) ok = swan_svm_fail(vm, "SVM PHI has no incoming value for predecessor");
                    break;
                }
                case SWAN_IR_CALL: {
                    size_t callee = vm->module->function_count;
                    for (size_t k = 0; k < vm->module->function_count; ++k)
                        if (!strcmp(vm->module->functions[k].name, in->symbol)) { callee = k; break; }
                    if (callee == vm->module->function_count) { ok = swan_svm_fail(vm, "SVM unresolved function call"); break; }
                    if (in->incoming_count > 16) { ok = swan_svm_fail(vm, "SVM call has too many arguments"); break; }
                    int64_t args[16] = {0};
                    for (size_t k = 0; k < in->incoming_count; ++k) {
                        uint32_t id = in->incoming[k].value;
                        if (id >= value_count) { ok = swan_svm_fail(vm, "SVM call argument out of range"); break; }
                        args[k] = values[id];
                    }
                    if (ok && !swan_svm_eval(vm, callee, args, in->incoming_count, &v)) ok = false;
                    break;
                }
                case SWAN_IR_ALLOCA: v=values[in->result];break;
                case SWAN_IR_LOAD:
                    if(a<=0||(uint64_t)a>memory_count){ok=swan_svm_fail(vm,"SVM invalid memory load");break;}
                    v=memory[a];break;
                case SWAN_IR_STORE:
                    if(a<=0||(uint64_t)a>memory_count){ok=swan_svm_fail(vm,"SVM invalid memory store");break;}
                    memory[a]=b;break;
                default: ok = swan_svm_fail(vm, "SVM encountered an unsupported IR opcode"); break;
            }
            if (ok && in->result) {
                if (in->result >= value_count) ok = swan_svm_fail(vm, "SVM destination register out of range");
                else values[in->result] = v;
            }
        }
        if (!ok) break;
        const SwanIRTerm *term = &block->terminator;
        switch (term->kind) {
            case SWAN_IR_BR: predecessor = block_index; block_index = term->target; break;
            case SWAN_IR_COND_BR:
                predecessor = block_index;
                if (term->value >= value_count) ok = swan_svm_fail(vm, "SVM branch condition out of range");
                else block_index = values[term->value] ? term->target : term->alternate;
                break;
            case SWAN_IR_RET:
                if (term->value >= value_count) ok = swan_svm_fail(vm, "SVM return value out of range");
                else if (result) *result = term->value ? values[term->value] : 0;
                returned = true; break;
            case SWAN_IR_UNREACHABLE: ok = swan_svm_fail(vm, "SVM reached an unreachable block"); break;
            default: ok = swan_svm_fail(vm, "SVM encountered an invalid terminator"); break;
        }
    }
    --vm->depth;
    free(values);
    return ok;
}

static inline bool swan_svm_run(const SwanIRModule *module, const char *entry,
                                uint64_t fuel, int64_t *result,
                                char *error, size_t error_capacity) {
    char verify_error[256];
    if (!swan_ir_verify(module, verify_error, sizeof(verify_error))) {
        if (error && error_capacity) snprintf(error, error_capacity, "invalid IR: %s", verify_error);
        return false;
    }
    size_t index = module->function_count;
    for (size_t i = 0; i < module->function_count; ++i)
        if (!strcmp(module->functions[i].name, entry)) { index = i; break; }
    if (index == module->function_count) {
        if (error && error_capacity) snprintf(error, error_capacity, "SVM entry function not found");
        return false;
    }
    SwanVM vm = {module, fuel ? fuel : UINT64_MAX, 0, error, error_capacity};
    return swan_svm_eval(&vm, index, NULL, 0, result);
}

/* Portable SVM module format. All integers are encoded explicitly in
 * little-endian order; no host struct layout or pointer value is serialized. */
#define SWAN_SVM_MAX_ITEMS 1000000u
typedef struct { FILE *file; uint64_t remaining; bool failed; } SwanSVMReader;

static inline bool swan_svm_write_bytes(FILE *f,const void *p,size_t n) {
    return n==0 || fwrite(p,1,n,f)==n;
}
static inline bool swan_svm_write_u8(FILE *f,uint8_t v) { return swan_svm_write_bytes(f,&v,1); }
static inline bool swan_svm_write_u32(FILE *f,uint32_t v) {
    uint8_t b[4];for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(v>>(i*8));
    return swan_svm_write_bytes(f,b,4);
}
static inline bool swan_svm_write_u64(FILE *f,uint64_t v) {
    uint8_t b[8];for(unsigned i=0;i<8;i++)b[i]=(uint8_t)(v>>(i*8));
    return swan_svm_write_bytes(f,b,8);
}
static inline bool swan_svm_write_string(FILE *f,const char *s) {
    size_t n=s?strlen(s):0;if(n>UINT32_MAX)return false;
    return swan_svm_write_u32(f,(uint32_t)n)&&swan_svm_write_bytes(f,s,n);
}
static inline bool swan_svm_read_bytes(SwanSVMReader *r,void *p,size_t n) {
    if(r->failed||n>r->remaining){r->failed=true;return false;}
    if(n&&fread(p,1,n,r->file)!=n){r->failed=true;return false;}
    r->remaining-=n;return true;
}
static inline bool swan_svm_read_u8(SwanSVMReader *r,uint8_t *v) { return swan_svm_read_bytes(r,v,1); }
static inline bool swan_svm_read_u32(SwanSVMReader *r,uint32_t *v) {
    uint8_t b[4];if(!swan_svm_read_bytes(r,b,4))return false;
    *v=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);return true;
}
static inline bool swan_svm_read_u64(SwanSVMReader *r,uint64_t *v) {
    uint8_t b[8];if(!swan_svm_read_bytes(r,b,8))return false;
    *v=0;for(unsigned i=0;i<8;i++)*v|=(uint64_t)b[i]<<(i*8);return true;
}
static inline char *swan_svm_read_string(SwanSVMReader *r) {
    uint32_t n;if(!swan_svm_read_u32(r,&n)||n>r->remaining||n>16u*1024u*1024u)return NULL;
    char *s=malloc((size_t)n+1);if(!s)return NULL;
    if(!swan_svm_read_bytes(r,s,n)){free(s);return NULL;}s[n]=0;return s;
}
static inline bool swan_svm_write_module(FILE *out,const SwanIRModule *m) {
    char verify[256];if(!out||!swan_ir_verify(m,verify,sizeof(verify)))return false;
    FILE *payload=tmpfile();if(!payload)return false;
    bool ok=swan_svm_write_u32(payload,(uint32_t)m->function_count);
    for(size_t f=0;ok&&f<m->function_count;f++){
        const SwanIRFunction *fn=&m->functions[f];
        if(fn->parameter_count>SWAN_SVM_MAX_ITEMS||fn->block_count>SWAN_SVM_MAX_ITEMS){ok=false;break;}
        ok=swan_svm_write_string(payload,fn->name)&&swan_svm_write_u8(payload,(uint8_t)fn->return_type)&&swan_svm_write_u32(payload,(uint32_t)fn->parameter_count);
        for(size_t p=0;ok&&p<fn->parameter_count;p++)ok=swan_svm_write_u8(payload,(uint8_t)fn->parameter_types[p]);
        ok=ok&&swan_svm_write_u32(payload,(uint32_t)fn->block_count)&&swan_svm_write_u32(payload,fn->next_value);
        for(size_t b=0;ok&&b<fn->block_count;b++){
            const SwanIRBlock *bb=&fn->blocks[b];
            if(bb->instruction_count>SWAN_SVM_MAX_ITEMS){ok=false;break;}
            ok=swan_svm_write_string(payload,bb->name)&&swan_svm_write_u32(payload,(uint32_t)bb->instruction_count);
            for(size_t x=0;ok&&x<bb->instruction_count;x++){
                const SwanIRInst *in=&bb->instructions[x];
                if(in->incoming_count>SWAN_SVM_MAX_ITEMS){ok=false;break;}
                ok=swan_svm_write_u8(payload,(uint8_t)in->op)&&swan_svm_write_u8(payload,(uint8_t)in->type)&&
                   swan_svm_write_u32(payload,in->result)&&swan_svm_write_u32(payload,in->a)&&
                   swan_svm_write_u32(payload,in->b)&&swan_svm_write_u32(payload,in->c)&&
                   swan_svm_write_u64(payload,(uint64_t)in->immediate)&&
                   swan_svm_write_string(payload,in->symbol)&&swan_svm_write_u32(payload,(uint32_t)in->incoming_count);
                for(size_t k=0;ok&&k<in->incoming_count;k++)
                    ok=swan_svm_write_u32(payload,in->incoming[k].value)&&swan_svm_write_u32(payload,in->incoming[k].block);
            }
            ok=ok&&swan_svm_write_u8(payload,(uint8_t)bb->terminator.kind)&&
               swan_svm_write_u32(payload,bb->terminator.value)&&
               swan_svm_write_u32(payload,bb->terminator.target)&&
               swan_svm_write_u32(payload,bb->terminator.alternate);
        }
    }
    long end=ok?ftell(payload):-1;
    if(end<0||fseek(payload,0,SEEK_SET)!=0)ok=false;
    static const uint8_t magic[8]={'S','W','A','N','V','M','0','1'};
    if(ok)ok=swan_svm_write_bytes(out,magic,8)&&swan_svm_write_u32(out,SWAN_SVM_VERSION)&&
             swan_svm_write_u32(out,0)&&swan_svm_write_u64(out,(uint64_t)end);
    uint8_t buffer[8192];while(ok){size_t n=fread(buffer,1,sizeof(buffer),payload);if(n&&!swan_svm_write_bytes(out,buffer,n)){ok=false;break;}if(n<sizeof(buffer)){if(ferror(payload))ok=false;break;}}
    fclose(payload);return ok&&fflush(out)==0;
}
static inline bool swan_svm_read_module(FILE *in,SwanIRModule *m,char *error,size_t cap) {
    if(!in||!m)return false;memset(m,0,sizeof(*m));
    uint8_t magic[8],expected[8]={'S','W','A','N','V','M','0','1'};
    uint32_t version,flags;uint64_t size;
    if(fread(magic,1,8,in)!=8||memcmp(magic,expected,8)) {
        if(error&&cap)snprintf(error,cap,"invalid SVM module header");return false;
    }
    SwanSVMReader header={in,UINT64_MAX,false};
    if(!swan_svm_read_u32(&header,&version)||!swan_svm_read_u32(&header,&flags)||!swan_svm_read_u64(&header,&size)||version!=SWAN_SVM_VERSION||flags!=0||size>UINT32_MAX*UINT64_C(16)){
        if(error&&cap)snprintf(error,cap,"unsupported or malformed SVM module header");return false;
    }
    SwanSVMReader r={in,size,false};uint32_t functions=0;
    if(!swan_svm_read_u32(&r,&functions)||functions>SWAN_SVM_MAX_ITEMS)goto malformed;
    for(uint32_t f=0;f<functions;f++){
        char *name=swan_svm_read_string(&r);uint8_t ret;uint32_t params,blocks,next;
        if(!name||!swan_svm_read_u8(&r,&ret)||!swan_svm_read_u32(&r,&params)||params>1024){free(name);goto malformed;}
        SwanIRType *types=params?malloc((size_t)params*sizeof(*types)):NULL;
        if(params&&!types){free(name);goto malformed;}
        bool ok=true;for(uint32_t p=0;p<params;p++){uint8_t t;if(!swan_svm_read_u8(&r,&t)){ok=false;break;}types[p]=(SwanIRType)t;}
        SwanIRFunction *fn=ok?swan_ir_add_function(m,name,(SwanIRType)ret,types,params):NULL;free(types);
        if(!fn){free(name);goto malformed;}
        if(!swan_svm_read_u32(&r,&blocks)||blocks>SWAN_SVM_MAX_ITEMS||!swan_svm_read_u32(&r,&next))goto malformed;
        fn->next_value=next;
        for(uint32_t b=0;b<blocks;b++){
            char *block_name=swan_svm_read_string(&r);uint32_t count;
            if(!block_name||!swan_svm_read_u32(&r,&count)||count>SWAN_SVM_MAX_ITEMS){free(block_name);goto malformed;}
            SwanIRBlock *bb=swan_ir_add_block(fn,block_name);if(!bb){free(block_name);goto malformed;}
            for(uint32_t x=0;x<count;x++){
                SwanIRInst in={0};uint8_t op,type;uint64_t imm;uint32_t incoming;
                if(!swan_svm_read_u8(&r,&op)||!swan_svm_read_u8(&r,&type)||
                   !swan_svm_read_u32(&r,&in.result)||!swan_svm_read_u32(&r,&in.a)||
                   !swan_svm_read_u32(&r,&in.b)||!swan_svm_read_u32(&r,&in.c)||
                   !swan_svm_read_u64(&r,&imm))goto malformed;
                in.op=(SwanIROp)op;in.type=(SwanIRType)type;in.immediate=(int64_t)imm;
                in.symbol=swan_svm_read_string(&r);if(r.failed)goto malformed;
                if(!swan_svm_read_u32(&r,&incoming)||incoming>SWAN_SVM_MAX_ITEMS){free((void*)in.symbol);goto malformed;}
                if(incoming){in.incoming=calloc(incoming,sizeof(*in.incoming));if(!in.incoming){free((void*)in.symbol);goto malformed;}}
                in.incoming_count=incoming;
                for(uint32_t k=0;k<incoming;k++)if(!swan_svm_read_u32(&r,&in.incoming[k].value)||!swan_svm_read_u32(&r,&in.incoming[k].block)){free((void*)in.symbol);free(in.incoming);goto malformed;}
                if(!swan_ir_reserve((void**)&bb->instructions,&bb->instruction_capacity,bb->instruction_count+1,sizeof(*bb->instructions))){free((void*)in.symbol);free(in.incoming);goto malformed;}
                bb->instructions[bb->instruction_count++]=in;
            }
            uint8_t term;SwanIRTerm t={0};
            if(!swan_svm_read_u8(&r,&term)||!swan_svm_read_u32(&r,&t.value)||!swan_svm_read_u32(&r,&t.target)||!swan_svm_read_u32(&r,&t.alternate))goto malformed;
            t.kind=(SwanIRTermKind)term;bb->terminator=t;bb->terminated=true;
        }
    }
    if(r.failed||r.remaining!=0)goto malformed;
    if(!swan_ir_verify(m,error,cap)){swan_ir_destroy(m);return false;}
    if(error&&cap)error[0]=0;return true;
malformed:
    swan_ir_destroy(m);if(error&&cap)snprintf(error,cap,"truncated or malformed SVM module payload");return false;
}
static inline bool swan_svm_run_file(const char *path,const char *entry,uint64_t fuel,
                                     int64_t *result,char *error,size_t cap) {
    FILE *f=fopen(path,"rb");if(!f){if(error&&cap)snprintf(error,cap,"cannot open SVM module");return false;}
    SwanIRModule module={0};bool ok=swan_svm_read_module(f,&module,error,cap);fclose(f);
    if(ok)ok=swan_svm_run(&module,entry,fuel,result,error,cap);
    swan_ir_destroy(&module);return ok;
}

#endif
