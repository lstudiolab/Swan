#ifndef BREW_OPTIMIZATION_H
#define BREW_OPTIMIZATION_H
#include "brew/ir.h"

/* Target-independent, semantics-preserving scalar IR optimizations. */
/* Debug builds validate every pass boundary. Release builds retain entry
 * and exit verification while avoiding repeated full-module verifier walks. */
static inline bool brew_ir_verify_pass_result(const BrewIRModule *module,char *error,size_t cap){
#if !defined(NDEBUG) || defined(BREW_IR_VERIFY_EACH_PASS)
    return brew_ir_verify(module,error,cap);
#else
    (void)module;(void)error;(void)cap;
    return true;
#endif
}
static inline bool brew_ir_constant(const BrewIRFunction *fn, uint32_t id,
                                    int64_t *value) {
    if (!id || id <= fn->parameter_count) return false;
    for (size_t b=0; b<fn->block_count; ++b)
        for (size_t i=0; i<fn->blocks[b].instruction_count; ++i) {
            const BrewIRInst *in=&fn->blocks[b].instructions[i];
            if (in->result==id && in->op==BREW_IR_CONST) { *value=in->immediate; return true; }
            if (in->result==id) return false;
        }
    return false;
}
static inline unsigned brew_ir_integer_bits(BrewIRType t){
    switch(t){case BREW_IR_I1:return 1;case BREW_IR_I8:return 8;case BREW_IR_I16:return 16;case BREW_IR_I32:return 32;default:return 64;}
}
static inline uint64_t brew_ir_width_mask(unsigned bits){return bits>=64?UINT64_MAX:(UINT64_C(1)<<bits)-1;}
static inline int64_t brew_ir_signed_value(uint64_t value,unsigned bits){
    /* i1 is a boolean domain (0 or 1), not a signed one-bit integer (-1 or 0).
       Keeping true canonical prevents constant folding from emitting invalid
       i1 constants that the verifier correctly rejects. */
    if(bits==1)return (int64_t)(value&UINT64_C(1));
    if(bits>=64)return (int64_t)value;
    uint64_t sign=UINT64_C(1)<<(bits-1);value&=brew_ir_width_mask(bits);
    return (int64_t)((value^sign)-sign);
}
static inline bool brew_ir_constant_lookup(const BrewIRFunction *fn,uint32_t id,
        int64_t *value,const unsigned char *known,const int64_t *values){
    if(known&&values){
        if(!id||id>=fn->next_value||!known[id])return false;
        *value=values[id];return true;
    }
    return brew_ir_constant(fn,id,value);
}
static inline bool brew_ir_build_constant_table(const BrewIRFunction *fn,
        unsigned char **known_out,int64_t **values_out,char *error,size_t cap){
    size_t count=fn->next_value;
    if(count>SIZE_MAX/sizeof(int64_t)){
        if(error&&cap)snprintf(error,cap,"constant table size overflow");
        return false;
    }
    unsigned char *known=calloc(count,sizeof(*known));
    int64_t *values=calloc(count,sizeof(*values));
    if(!known||!values){
        free(known);free(values);
        if(error&&cap)snprintf(error,cap,"out of memory building constant table");
        return false;
    }
    for(size_t b=0;b<fn->block_count;b++)
        for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
            const BrewIRInst *in=&fn->blocks[b].instructions[i];
            if(in->op==BREW_IR_CONST&&in->result&&in->result<count){
                known[in->result]=1;values[in->result]=in->immediate;
            }
        }
    *known_out=known;*values_out=values;
    return true;
}
static inline bool brew_ir_fold_instruction_using(BrewIRFunction *fn,BrewIRInst *in,
        const unsigned char *known,const int64_t *values){
    if(in->op==BREW_IR_SELECT){
        int64_t condition,yes,no;
        if(brew_ir_constant_lookup(fn,in->a,&condition,known,values)&&brew_ir_constant_lookup(fn,in->b,&yes,known,values)&&brew_ir_constant_lookup(fn,in->c,&no,known,values)){
            in->op=BREW_IR_CONST;in->a=in->b=in->c=0;in->immediate=condition?yes:no;return true;
        }
        return false;
    }
    if(in->op==BREW_IR_CONST||in->op==BREW_IR_PHI||in->op==BREW_IR_CALL||
       in->op==BREW_IR_ALLOCA||in->op==BREW_IR_LOAD||in->op==BREW_IR_STORE||in->op==BREW_IR_SELECT)return false;
    int64_t a,b;if(!brew_ir_constant_lookup(fn,in->a,&a,known,values)||!brew_ir_constant_lookup(fn,in->b,&b,known,values))return false;
    unsigned bits=brew_ir_integer_bits(brew_ir_value_type_of(fn,in->a));
    uint64_t mask=brew_ir_width_mask(bits),ua=(uint64_t)a&mask,ub=(uint64_t)b&mask;
    int64_t sa=brew_ir_signed_value(ua,bits),sb=brew_ir_signed_value(ub,bits),value=0;bool fold=true;
    switch(in->op){
        case BREW_IR_ADD:value=brew_ir_signed_value((ua+ub)&mask,bits);break;
        case BREW_IR_SUB:value=brew_ir_signed_value((ua-ub)&mask,bits);break;
        case BREW_IR_MUL:value=brew_ir_signed_value((ua*ub)&mask,bits);break;
        case BREW_IR_SDIV:if(!sb||(bits==64&&sa==INT64_MIN&&sb==-1)||(bits<64&&sa==-(INT64_C(1)<<(bits-1))&&sb==-1))fold=false;else value=brew_ir_signed_value((uint64_t)(sa/sb)&mask,bits);break;
        case BREW_IR_UDIV:if(!ub)fold=false;else value=brew_ir_signed_value((ua/ub)&mask,bits);break;
        case BREW_IR_SREM:if(!sb||(bits==64&&sa==INT64_MIN&&sb==-1)||(bits<64&&sa==-(INT64_C(1)<<(bits-1))&&sb==-1))fold=false;else value=brew_ir_signed_value((uint64_t)(sa%sb)&mask,bits);break;
        case BREW_IR_UREM:if(!ub)fold=false;else value=brew_ir_signed_value((ua%ub)&mask,bits);break;
        case BREW_IR_AND:value=brew_ir_signed_value(ua&ub,bits);break;
        case BREW_IR_OR:value=brew_ir_signed_value(ua|ub,bits);break;
        case BREW_IR_XOR:value=brew_ir_signed_value(ua^ub,bits);break;
        case BREW_IR_SHL:value=brew_ir_signed_value((ua<<((uint64_t)b&(bits-1u)))&mask,bits);break;
        case BREW_IR_LSHR:value=brew_ir_signed_value(ua>>((uint64_t)b&(bits-1u)),bits);break;
        case BREW_IR_ASHR:{unsigned shift=(unsigned)((uint64_t)b&(bits-1u));uint64_t fill=(ua>>(bits-1u))?mask:0;uint64_t shifted=shift?(ua>>shift)|(fill<<(bits-shift)):ua;value=brew_ir_signed_value(shifted,bits);break;}
        case BREW_IR_ICMP_EQ:value=ua==ub;break;case BREW_IR_ICMP_NE:value=ua!=ub;break;
        case BREW_IR_ICMP_SLT:value=sa<sb;break;case BREW_IR_ICMP_SLE:value=sa<=sb;break;
        case BREW_IR_ICMP_SGT:value=sa>sb;break;case BREW_IR_ICMP_SGE:value=sa>=sb;break;
        default:fold=false;break;
    }
    if(!fold)return false;
    in->op=BREW_IR_CONST;in->a=in->b=in->c=0;in->immediate=value;return true;
}
static inline bool brew_ir_fold_instruction(BrewIRFunction *fn,BrewIRInst *in){
    return brew_ir_fold_instruction_using(fn,in,NULL,NULL);
}
static inline void brew_ir_replace_uses(BrewIRFunction *fn,uint32_t from,uint32_t to){
    for(size_t b=0;b<fn->block_count;b++){
        BrewIRBlock *bb=&fn->blocks[b];
        for(size_t i=0;i<bb->instruction_count;i++){
            BrewIRInst *in=&bb->instructions[i];
            if(in->a==from)in->a=to;
            if(in->b==from)in->b=to;
            if(in->c==from)in->c=to;
            for(size_t k=0;k<in->incoming_count;k++)if(in->incoming[k].value==from)in->incoming[k].value=to;
        }
        if(bb->terminator.value==from)bb->terminator.value=to;
    }
}
static inline bool brew_ir_pure_cse_op(BrewIROp op){
    return op==BREW_IR_ADD||op==BREW_IR_SUB||op==BREW_IR_MUL||
        op==BREW_IR_SDIV||op==BREW_IR_UDIV||op==BREW_IR_SREM||op==BREW_IR_UREM||
        op==BREW_IR_AND||op==BREW_IR_OR||op==BREW_IR_XOR||
        op==BREW_IR_SHL||op==BREW_IR_LSHR||op==BREW_IR_ASHR||
        (op>=BREW_IR_ICMP_EQ&&op<=BREW_IR_ICMP_SGE)||op==BREW_IR_SELECT;
}
static inline bool brew_ir_commutative(BrewIROp op){
    return op==BREW_IR_ADD||op==BREW_IR_MUL||op==BREW_IR_AND||op==BREW_IR_OR||
        op==BREW_IR_XOR||op==BREW_IR_ICMP_EQ||op==BREW_IR_ICMP_NE;
}
typedef struct {
    uint64_t hash;
    uint32_t a,b,c,result;
    size_t block,index,next;
    BrewIROp op;
    BrewIRType type;
} BrewIRCSEEntry;
static inline uint64_t brew_ir_cse_hash(BrewIROp op,BrewIRType type,uint32_t a,uint32_t b,uint32_t c){
    uint64_t fields[5]={(uint64_t)op,(uint64_t)type,a,b,c};
    uint64_t hash=UINT64_C(1469598103934665603);
    for(size_t i=0;i<5;i++){hash^=fields[i];hash*=UINT64_C(1099511628211);}
    return hash;
}
static inline uint32_t brew_ir_resolve_replacement(uint32_t value,const uint32_t *map,size_t count){
    size_t steps=0;
    while(value&&value<count&&map[value]!=value&&steps++<count)value=map[value];
    return value<count?value:0;
}
static inline bool brew_ir_common_subexpression_eliminate(BrewIRModule *module,bool *changed_out,char *error,size_t cap){
    bool changed=false;
    for(size_t f=0;f<module->function_count;f++){
        BrewIRFunction *fn=&module->functions[f];
        size_t total=0,value_count=fn->next_value;
        for(size_t b=0;b<fn->block_count;b++){
            if(fn->blocks[b].instruction_count>SIZE_MAX-total){
                if(error&&cap)snprintf(error,cap,"CSE instruction count overflow");
                return false;
            }
            total+=fn->blocks[b].instruction_count;
        }
        if(!total)continue;
        if(total>SIZE_MAX/sizeof(BrewIRCSEEntry)||value_count>SIZE_MAX/sizeof(uint32_t)||total>SIZE_MAX/2){
            if(error&&cap)snprintf(error,cap,"CSE analysis size overflow");
            return false;
        }
        size_t bucket_count=1;
        while(bucket_count<total*2){
            if(bucket_count>SIZE_MAX/2){
                if(error&&cap)snprintf(error,cap,"CSE hash table size overflow");
                return false;
            }
            bucket_count*=2;
        }
        if(bucket_count>SIZE_MAX/sizeof(size_t)){
            if(error&&cap)snprintf(error,cap,"CSE bucket allocation overflow");
            return false;
        }
        BrewIRCSEEntry *entries=malloc(total*sizeof(*entries));
        size_t *heads=malloc(bucket_count*sizeof(*heads));
        uint32_t *replacement=malloc(value_count*sizeof(*replacement));
        if(!entries||!heads||!replacement){
            free(entries);free(heads);free(replacement);
            if(error&&cap)snprintf(error,cap,"out of memory during common-subexpression elimination");
            return false;
        }
        for(size_t i=0;i<bucket_count;i++)heads[i]=SIZE_MAX;
        for(size_t id=0;id<value_count;id++)replacement[id]=(uint32_t)id;
        size_t entry_count=0;bool function_changed=false;
        for(size_t b=0;b<fn->block_count;b++){
            BrewIRBlock *bb=&fn->blocks[b];
            for(size_t i=0;i<bb->instruction_count;i++){
                BrewIRInst *in=&bb->instructions[i];
                if(!in->result||in->result>=value_count||!brew_ir_pure_cse_op(in->op))continue;
                uint32_t a=brew_ir_resolve_replacement(in->a,replacement,value_count);
                uint32_t z=brew_ir_resolve_replacement(in->b,replacement,value_count);
                uint32_t c=brew_ir_resolve_replacement(in->c,replacement,value_count);
                if(brew_ir_commutative(in->op)&&a>z){uint32_t t=a;a=z;z=t;}
                uint64_t hash=brew_ir_cse_hash(in->op,in->type,a,z,c);
                size_t bucket=(size_t)(hash&(uint64_t)(bucket_count-1));
                size_t candidate=heads[bucket];
                bool found=false;
                while(candidate!=SIZE_MAX){
                    const BrewIRCSEEntry *entry=&entries[candidate];
                    if(entry->hash==hash&&entry->op==in->op&&entry->type==in->type&&
                       entry->a==a&&entry->b==z&&entry->c==c){
                        bool dominates=entry->block==b?entry->index<i:
                            brew_ir_block_dominates(fn,entry->block,b);
                        if(dominates){
                            replacement[in->result]=entry->result;
                            changed=true;function_changed=true;found=true;break;
                        }
                    }
                    candidate=entry->next;
                }
                if(!found){
                    BrewIRCSEEntry *entry=&entries[entry_count];
                    entry->hash=hash;entry->a=a;entry->b=z;entry->c=c;
                    entry->result=in->result;entry->block=b;entry->index=i;
                    entry->op=in->op;entry->type=in->type;entry->next=heads[bucket];
                    heads[bucket]=entry_count++;
                }
            }
        }
        if(function_changed){
            for(size_t b=0;b<fn->block_count;b++){
                BrewIRBlock *bb=&fn->blocks[b];
                for(size_t i=0;i<bb->instruction_count;i++){
                    BrewIRInst *in=&bb->instructions[i];
                    if(in->a<value_count)in->a=brew_ir_resolve_replacement(in->a,replacement,value_count);
                    if(in->b<value_count)in->b=brew_ir_resolve_replacement(in->b,replacement,value_count);
                    if(in->c<value_count)in->c=brew_ir_resolve_replacement(in->c,replacement,value_count);
                    for(size_t k=0;k<in->incoming_count;k++)
                        if(in->incoming[k].value<value_count)
                            in->incoming[k].value=brew_ir_resolve_replacement(in->incoming[k].value,replacement,value_count);
                }
                if(bb->terminator.value<value_count)
                    bb->terminator.value=brew_ir_resolve_replacement(bb->terminator.value,replacement,value_count);
            }
        }
        free(entries);free(heads);free(replacement);
    }
    if(changed_out)*changed_out=changed;
    return true;
}
static inline bool brew_ir_algebraic_simplify_function(BrewIRFunction *fn,bool *changed_out,char *error,size_t cap){
    bool changed=false;
    size_t count=fn->next_value;
    if(count>SIZE_MAX/sizeof(int64_t)||count>SIZE_MAX/sizeof(BrewIRInst*)){
        if(error&&cap)snprintf(error,cap,"algebraic analysis size overflow");
        return false;
    }
    unsigned char *known=calloc(count,sizeof(*known));
    int64_t *values=calloc(count,sizeof(*values));
    BrewIRInst **definitions=calloc(count,sizeof(*definitions));
    if(!known||!values||!definitions){
        free(known);free(values);free(definitions);
        if(error&&cap)snprintf(error,cap,"out of memory during algebraic analysis");
        return false;
    }
    for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
        BrewIRInst *in=&fn->blocks[b].instructions[i];
        if(in->result&&in->result<count)definitions[in->result]=in;
    }
    for(uint32_t id=(uint32_t)fn->parameter_count+1;id<fn->next_value;id++){
        BrewIRInst *in=definitions[id];
        if(!in)continue;
        if(in->op==BREW_IR_CONST){known[id]=1;values[id]=in->immediate;continue;}
        if(!in->result||!brew_ir_pure_cse_op(in->op))continue;
        if(in->op==BREW_IR_SELECT)continue;
        int64_t a=0,z=0;
        bool ca=brew_ir_constant_lookup(fn,in->a,&a,known,values);
        bool cz=brew_ir_constant_lookup(fn,in->b,&z,known,values);
        uint32_t replacement=0;
        bool make_all_ones=false;
        unsigned bits=brew_ir_integer_bits(in->type);
        uint64_t all_ones=brew_ir_width_mask(bits);
        bool safe_a=in->a<=fn->parameter_count||
            (in->a<count&&definitions[in->a]&&definitions[in->a]->op==BREW_IR_CONST);
        bool safe_b=in->b<=fn->parameter_count||
            (in->b<count&&definitions[in->b]&&definitions[in->b]->op==BREW_IR_CONST);
        /*
         * Operand-dropping rewrites are legal only for parameters and
         * constants. A computed value may depend on a trapping operation;
         * replacing its consumer could let DCE erase that trap.
         */
        if(cz&&((uint64_t)z&all_ones)==all_ones&&in->op==BREW_IR_OR&&safe_a)make_all_ones=true;
        else if(ca&&((uint64_t)a&all_ones)==all_ones&&in->op==BREW_IR_OR&&safe_b)make_all_ones=true;
        else if(cz&&z==0&&(in->op==BREW_IR_ADD||in->op==BREW_IR_SUB||in->op==BREW_IR_OR||
                in->op==BREW_IR_XOR||in->op==BREW_IR_SHL||in->op==BREW_IR_LSHR||in->op==BREW_IR_ASHR))replacement=in->a;
        else if(ca&&a==0&&(in->op==BREW_IR_ADD||in->op==BREW_IR_OR||in->op==BREW_IR_XOR))replacement=in->b;
        else if(cz&&z==1&&(in->op==BREW_IR_MUL||in->op==BREW_IR_SDIV||in->op==BREW_IR_UDIV))replacement=in->a;
        else if(ca&&a==1&&in->op==BREW_IR_MUL)replacement=in->b;
        else if(cz&&((uint64_t)z&all_ones)==all_ones&&in->op==BREW_IR_AND)replacement=in->a;
        else if(ca&&((uint64_t)a&all_ones)==all_ones&&in->op==BREW_IR_AND)replacement=in->b;
        if(replacement){
            brew_ir_replace_uses(fn,in->result,replacement);
            changed=true;
        }else if(make_all_ones){
            in->op=BREW_IR_CONST;
            in->a=in->b=in->c=0;
            in->immediate=brew_ir_signed_value(all_ones,bits);
            known[id]=1;
            values[id]=in->immediate;
            changed=true;
        }
    }
    free(known);free(values);free(definitions);
    if(changed_out)*changed_out=changed;
    return true;
}
static inline bool brew_ir_algebraic_simplify(BrewIRModule *m,bool *changed_out,char *error,size_t cap){
    bool changed=false;
    for(size_t f=0;f<m->function_count;f++){
        bool function_changed=false;
        if(!brew_ir_algebraic_simplify_function(&m->functions[f],&function_changed,error,cap))return false;
        changed=function_changed||changed;
    }
    if(changed_out)*changed_out=changed;
    return true;
}
static inline size_t brew_ir_use_count(const BrewIRFunction *fn,uint32_t value){
    size_t uses=0;
    for(size_t b=0;b<fn->block_count;b++){
        const BrewIRBlock *bb=&fn->blocks[b];
        for(size_t i=0;i<bb->instruction_count;i++){
            const BrewIRInst *in=&bb->instructions[i];
            uses+=(in->a==value)+(in->b==value)+(in->c==value);
            for(size_t k=0;k<in->incoming_count;k++)uses+=in->incoming[k].value==value;
        }
        uses+=bb->terminator.value==value;
    }
    return uses;
}
static inline bool brew_ir_inst_has_side_effect(const BrewIRInst *in){
    return in->op==BREW_IR_CALL||in->op==BREW_IR_STORE||in->op==BREW_IR_LOAD||in->op==BREW_IR_SDIV||in->op==BREW_IR_UDIV||in->op==BREW_IR_SREM||in->op==BREW_IR_UREM;
}
static inline void brew_ir_mark_live_value(const BrewIRFunction *fn,uint32_t value,
        unsigned char *live,uint32_t *work,size_t *top){
    if(!value||value>=fn->next_value||live[value])return;
    live[value]=1;work[(*top)++]=value;
}
static inline bool brew_ir_dead_code_eliminate(BrewIRModule *module,bool *changed_out){
    bool changed=false;
    for(size_t f=0;f<module->function_count;f++){
        BrewIRFunction *fn=&module->functions[f];
        size_t value_count=fn->next_value;
        if(!value_count)continue;
        if(value_count>SIZE_MAX/sizeof(uint32_t)||value_count>SIZE_MAX/sizeof(BrewIRInst*))return false;
        const BrewIRInst **definitions=calloc(value_count,sizeof(*definitions));
        unsigned char *live=calloc(value_count,sizeof(*live));
        uint32_t *work=malloc(value_count*sizeof(*work));
        if(!definitions||!live||!work){free(definitions);free(live);free(work);return false;}
        for(size_t b=0;b<fn->block_count;b++)
            for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
                const BrewIRInst *in=&fn->blocks[b].instructions[i];
                if(in->result&&in->result<value_count)definitions[in->result]=in;
            }
        size_t top=0;
        for(size_t b=0;b<fn->block_count;b++){
            const BrewIRBlock *bb=&fn->blocks[b];
            const BrewIRTerm *term=&bb->terminator;
            brew_ir_mark_live_value(fn,term->value,live,work,&top);
            for(size_t i=0;i<bb->instruction_count;i++){
                const BrewIRInst *in=&bb->instructions[i];
                if(!brew_ir_inst_has_side_effect(in))continue;
                brew_ir_mark_live_value(fn,in->result,live,work,&top);
                brew_ir_mark_live_value(fn,in->a,live,work,&top);
                brew_ir_mark_live_value(fn,in->b,live,work,&top);
                brew_ir_mark_live_value(fn,in->c,live,work,&top);
                for(size_t k=0;k<in->incoming_count;k++)
                    brew_ir_mark_live_value(fn,in->incoming[k].value,live,work,&top);
            }
        }
        while(top){
            uint32_t id=work[--top];
            const BrewIRInst *in=definitions[id];
            if(!in)continue;
            brew_ir_mark_live_value(fn,in->a,live,work,&top);
            brew_ir_mark_live_value(fn,in->b,live,work,&top);
            brew_ir_mark_live_value(fn,in->c,live,work,&top);
            for(size_t k=0;k<in->incoming_count;k++)
                brew_ir_mark_live_value(fn,in->incoming[k].value,live,work,&top);
        }
        for(size_t b=0;b<fn->block_count;b++){
            BrewIRBlock *bb=&fn->blocks[b];size_t out=0;
            for(size_t i=0;i<bb->instruction_count;i++){
                BrewIRInst in=bb->instructions[i];
                bool keep=!in.result||brew_ir_inst_has_side_effect(&in)||in.result>=value_count||live[in.result];
                if(!keep){free(in.incoming);changed=true;continue;}
                bb->instructions[out++]=in;
            }
            bb->instruction_count=out;
        }
        free(definitions);free(live);free(work);
    }
    if(changed_out)*changed_out=changed;
    return true;
}
static inline bool brew_ir_cfg_simplify(BrewIRModule *module,bool *changed,char *error,size_t cap){
    *changed=false;
    for(size_t fi=0;fi<module->function_count;fi++){
        BrewIRFunction *fn=&module->functions[fi];size_t n=fn->block_count;
        unsigned char *constant_known=NULL;int64_t *constant_values=NULL;
        if(!brew_ir_build_constant_table(fn,&constant_known,&constant_values,error,cap))return false;
        for(size_t b=0;b<n;b++){
            BrewIRTerm *term=&fn->blocks[b].terminator;
            if(term->kind==BREW_IR_COND_BR){
                int64_t condition;
                if(brew_ir_constant_lookup(fn,term->value,&condition,constant_known,constant_values)){
                    term->kind=BREW_IR_BR;term->target=condition?term->target:term->alternate;term->alternate=0;term->value=0;*changed=true;
                }
            }
            if(term->kind==BREW_IR_COND_BR&&term->target==term->alternate){
                term->kind=BREW_IR_BR;term->alternate=0;term->value=0;*changed=true;
            }
        }
        free(constant_known);free(constant_values);
        if(n<=1)continue;
        bool *reachable=calloc(n,sizeof(*reachable));size_t *queue=malloc(n*sizeof(*queue));
        if(!reachable||!queue){free(reachable);free(queue);if(error&&cap)snprintf(error,cap,"out of memory simplifying IR control flow");return false;}
        size_t head=0,tail=0;reachable[0]=true;queue[tail++]=0;
        while(head<tail){
            size_t b=queue[head++];const BrewIRTerm *t=&fn->blocks[b].terminator;
            size_t targets[2],count=0;
            if(t->kind==BREW_IR_BR)targets[count++]=t->target;
            else if(t->kind==BREW_IR_COND_BR){targets[count++]=t->target;targets[count++]=t->alternate;}
            for(size_t k=0;k<count;k++)if(targets[k]<n&&!reachable[targets[k]]){reachable[targets[k]]=true;queue[tail++]=targets[k];}
        }
        size_t live=0;for(size_t b=0;b<n;b++)live+=reachable[b];
        if(live<n){
            size_t *map=malloc(n*sizeof(*map));BrewIRBlock *blocks=calloc(live,sizeof(*blocks));
            if(!map||!blocks){free(map);free(blocks);free(reachable);free(queue);if(error&&cap)snprintf(error,cap,"out of memory compacting IR control-flow graph");return false;}
            size_t next=0;for(size_t b=0;b<n;b++)map[b]=reachable[b]?next++:SIZE_MAX;
            for(size_t b=0;b<n;b++){
                if(reachable[b])blocks[map[b]]=fn->blocks[b];
                else{
                    for(size_t i=0;i<fn->blocks[b].instruction_count;i++)
                        free(fn->blocks[b].instructions[i].incoming);
                    free(fn->blocks[b].instructions);
                }
            }
            free(fn->blocks);fn->blocks=blocks;fn->block_count=fn->block_capacity=live;
            for(size_t b=0;b<live;b++){
                BrewIRBlock *bb=&fn->blocks[b];
                bb->owner=fn;
                bb->index=(uint32_t)b;
                BrewIRTerm *t=&bb->terminator;
                if(t->kind==BREW_IR_BR)t->target=(uint32_t)map[t->target];
                else if(t->kind==BREW_IR_COND_BR){t->target=(uint32_t)map[t->target];t->alternate=(uint32_t)map[t->alternate];}
                for(size_t i=0;i<bb->instruction_count;i++){
                    BrewIRInst *in=&bb->instructions[i];if(in->op!=BREW_IR_PHI)continue;
                    size_t out=0;
                    for(size_t k=0;k<in->incoming_count;k++)if(in->incoming[k].block<n&&map[in->incoming[k].block]!=SIZE_MAX){
                        in->incoming[out]=in->incoming[k];in->incoming[out++].block=(uint32_t)map[in->incoming[k].block];
                    }
                    in->incoming_count=out;
                }
            }
            free(map);*changed=true;
        }
        free(reachable);free(queue);
    }
    return true;
}
static inline bool brew_ir_copy_propagation(BrewIRModule *module,bool *changed_out,char *error,size_t cap){
    bool changed=false;
    for(size_t f=0;f<module->function_count;f++){
        BrewIRFunction *fn=&module->functions[f];
        size_t value_count=fn->next_value;
        if(value_count>SIZE_MAX/sizeof(uint32_t)){
            if(error&&cap)snprintf(error,cap,"copy-propagation map size overflow");
            return false;
        }
        uint32_t *replacement=malloc(value_count*sizeof(*replacement));
        if(!replacement){
            if(error&&cap)snprintf(error,cap,"out of memory during copy propagation");
            return false;
        }
        for(size_t id=0;id<value_count;id++)replacement[id]=(uint32_t)id;
        for(size_t b=0;b<fn->block_count;b++){
            BrewIRBlock *bb=&fn->blocks[b];
            for(size_t i=0;i<bb->instruction_count;i++){
                BrewIRInst *in=&bb->instructions[i];
                uint32_t source=0;
                if(in->op==BREW_IR_PHI&&in->incoming_count){
                    source=in->incoming[0].value;
                    for(size_t k=1;k<in->incoming_count;k++)
                        if(in->incoming[k].value!=source){source=0;break;}
                }else if(in->op==BREW_IR_SELECT&&in->b==in->c)source=in->b;
                if(in->result&&in->result<value_count&&source&&source<value_count&&source!=in->result)
                    replacement[in->result]=source;
            }
        }
        for(size_t id=1;id<value_count;id++){
            uint32_t current=(uint32_t)id;
            size_t steps=0;
            while(replacement[current]!=current){
                current=replacement[current];
                if(current>=value_count||++steps>=value_count){
                    free(replacement);
                    if(error&&cap)snprintf(error,cap,"cyclic copy-propagation replacement");
                    return false;
                }
            }
            if(replacement[id]!=current)changed=true;
            replacement[id]=current;
        }
        for(size_t b=0;b<fn->block_count;b++){
            BrewIRBlock *bb=&fn->blocks[b];
            for(size_t i=0;i<bb->instruction_count;i++){
                BrewIRInst *in=&bb->instructions[i];
                if(in->a<value_count)in->a=replacement[in->a];
                if(in->b<value_count)in->b=replacement[in->b];
                if(in->c<value_count)in->c=replacement[in->c];
                for(size_t k=0;k<in->incoming_count;k++)
                    if(in->incoming[k].value<value_count)
                        in->incoming[k].value=replacement[in->incoming[k].value];
            }
            if(bb->terminator.value<value_count)
                bb->terminator.value=replacement[bb->terminator.value];
        }
        free(replacement);
    }
    if(changed_out)*changed_out=changed;
    return true;
}

static inline bool brew_ir_licm_opcode(BrewIROp op){
    return op==BREW_IR_ADD||op==BREW_IR_SUB||op==BREW_IR_MUL||
        op==BREW_IR_AND||op==BREW_IR_OR||op==BREW_IR_XOR||
        op==BREW_IR_SHL||op==BREW_IR_LSHR||op==BREW_IR_ASHR||
        (op>=BREW_IR_ICMP_EQ&&op<=BREW_IR_ICMP_SGE)||op==BREW_IR_SELECT;
}
static inline bool brew_ir_value_defined_in_loop(const BrewIRFunction *fn,uint32_t value,
        const uint32_t *definition_blocks,const bool *loop){
    if(!value||value<=fn->parameter_count||value>=fn->next_value)return false;
    uint32_t block=definition_blocks[value];
    return block!=UINT32_MAX&&block<fn->block_count&&loop[block];
}
static inline bool brew_ir_loop_invariant_code_motion(BrewIRModule *module,bool *changed_out,char *error,size_t cap){
    bool changed=false;
    for(size_t fi=0;fi<module->function_count;fi++){
        BrewIRFunction *fn=&module->functions[fi];size_t n=fn->block_count;
        if(n<3)continue;
        size_t value_count=fn->next_value;
        if(value_count>SIZE_MAX/sizeof(uint32_t)){
            if(error&&cap)snprintf(error,cap,"LICM definition map size overflow");
            return false;
        }
        bool *loop=calloc(n,sizeof(*loop));
        size_t *queue=malloc(n*sizeof(*queue));
        uint32_t *definition_blocks=malloc(value_count*sizeof(*definition_blocks));
        uint32_t *next_definition_blocks=malloc(value_count*sizeof(*next_definition_blocks));
        if(!loop||!queue||!definition_blocks||!next_definition_blocks){
            free(loop);free(queue);free(definition_blocks);free(next_definition_blocks);
            if(error&&cap)snprintf(error,cap,"out of memory finding IR natural loops");
            return false;
        }
        for(size_t id=0;id<value_count;id++)definition_blocks[id]=UINT32_MAX;
        for(size_t b=0;b<n;b++)
            for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
                uint32_t result=fn->blocks[b].instructions[i].result;
                if(result&&result<value_count)definition_blocks[result]=(uint32_t)b;
            }
        for(size_t header=0;header<n;header++){
            memset(loop,0,n*sizeof(*loop));size_t head=0,tail=0;bool has_latch=false;
            loop[header]=true;
            for(size_t latch=0;latch<n;latch++){
                const BrewIRTerm *t=&fn->blocks[latch].terminator;
                bool backedge=(t->kind==BREW_IR_BR&&t->target==header)||
                    (t->kind==BREW_IR_COND_BR&&(t->target==header||t->alternate==header));
                if(backedge&&latch!=header&&brew_ir_block_dominates(fn,header,latch)){
                    if(!loop[latch]){loop[latch]=true;queue[tail++]=latch;}
                    has_latch=true;
                }
            }
            if(!has_latch)continue;
            while(head<tail){
                size_t node=queue[head++];
                for(size_t pred=0;pred<n;pred++){
                    const BrewIRTerm *t=&fn->blocks[pred].terminator;
                    bool edge=(t->kind==BREW_IR_BR&&t->target==node)||
                        (t->kind==BREW_IR_COND_BR&&(t->target==node||t->alternate==node));
                    if(edge&&pred!=header&&!loop[pred]&&brew_ir_block_dominates(fn,header,pred)){
                        loop[pred]=true;queue[tail++]=pred;
                    }
                }
            }
            size_t preheader=SIZE_MAX,outside_preds=0;
            for(size_t pred=0;pred<n;pred++){
                const BrewIRTerm *t=&fn->blocks[pred].terminator;
                bool edge=(t->kind==BREW_IR_BR&&t->target==header)||
                    (t->kind==BREW_IR_COND_BR&&(t->target==header||t->alternate==header));
                if(edge&&!loop[pred]){preheader=pred;outside_preds++;}
            }
            if(outside_preds!=1)continue;
            for(;;){
                size_t move_count=0;
                for(size_t b=0;b<n;b++)if(loop[b]){
                    const BrewIRBlock *bb=&fn->blocks[b];
                    for(size_t i=0;i<bb->instruction_count;i++){
                        const BrewIRInst *in=&bb->instructions[i];
                        if(!in->result||!brew_ir_licm_opcode(in->op)||
                           (in->type!=BREW_IR_I1&&in->type!=BREW_IR_I8&&in->type!=BREW_IR_I16&&in->type!=BREW_IR_I32&&in->type!=BREW_IR_I64))continue;
                        bool invariant=!brew_ir_value_defined_in_loop(fn,in->a,definition_blocks,loop);
                        if(in->op!=BREW_IR_SELECT)
                            invariant=invariant&&!brew_ir_value_defined_in_loop(fn,in->b,definition_blocks,loop);
                        else
                            invariant=invariant&&!brew_ir_value_defined_in_loop(fn,in->b,definition_blocks,loop)&&
                                !brew_ir_value_defined_in_loop(fn,in->c,definition_blocks,loop);
                        if(invariant)move_count++;
                    }
                }
                if(!move_count)break;
                BrewIRBlock *pre=&fn->blocks[preheader];
                if(move_count>SIZE_MAX-pre->instruction_count||
                   !brew_ir_reserve((void**)&pre->instructions,&pre->instruction_capacity,
                                    pre->instruction_count+move_count,sizeof(*pre->instructions))){
                    free(loop);free(queue);free(definition_blocks);free(next_definition_blocks);
                    if(error&&cap)snprintf(error,cap,"out of memory hoisting loop-invariant IR");
                    return false;
                }
                memcpy(next_definition_blocks,definition_blocks,value_count*sizeof(*definition_blocks));
                size_t moved=0;
                for(size_t b=0;b<n;b++)if(loop[b]){
                    BrewIRBlock *bb=&fn->blocks[b];size_t out=0;
                    for(size_t i=0;i<bb->instruction_count;i++){
                        BrewIRInst in=bb->instructions[i];
                        bool invariant=in.result&&brew_ir_licm_opcode(in.op)&&
                            (in.type==BREW_IR_I1||in.type==BREW_IR_I8||in.type==BREW_IR_I16||in.type==BREW_IR_I32||in.type==BREW_IR_I64)&&
                            !brew_ir_value_defined_in_loop(fn,in.a,definition_blocks,loop);
                        if(in.op==BREW_IR_SELECT)
                            invariant=invariant&&!brew_ir_value_defined_in_loop(fn,in.b,definition_blocks,loop)&&
                                !brew_ir_value_defined_in_loop(fn,in.c,definition_blocks,loop);
                        else
                            invariant=invariant&&!brew_ir_value_defined_in_loop(fn,in.b,definition_blocks,loop);
                        if(invariant){
                            pre->instructions[pre->instruction_count++]=in;
                            next_definition_blocks[in.result]=(uint32_t)preheader;
                            moved++;
                        }else{
                            bb->instructions[out++]=in;
                        }
                    }
                    bb->instruction_count=out;
                }
                if(moved!=move_count){
                    free(loop);free(queue);free(definition_blocks);free(next_definition_blocks);
                    if(error&&cap)snprintf(error,cap,"LICM candidate count changed during a hoist");
                    return false;
                }
                changed=true;
                uint32_t *swap=definition_blocks;
                definition_blocks=next_definition_blocks;
                next_definition_blocks=swap;
            }
        }
        free(loop);free(queue);free(definition_blocks);free(next_definition_blocks);
    }
    if(changed_out)*changed_out=changed;
    return brew_ir_verify_pass_result(module,error,cap);
}
typedef enum { BREW_IR_PASS_CONSTANT_FOLD, BREW_IR_PASS_CFG_SIMPLIFY, BREW_IR_PASS_ALGEBRAIC, BREW_IR_PASS_COPY_PROPAGATION, BREW_IR_PASS_COMMON_SUBEXPRESSION, BREW_IR_PASS_LOOP_INVARIANT, BREW_IR_PASS_DEAD_CODE } BrewIRPassKind;
typedef struct { BrewIRPassKind kind; const char *name; } BrewIRPass;
static const BrewIRPass brew_ir_default_passes[]={
    {BREW_IR_PASS_CONSTANT_FOLD,"constant-folding"},
    {BREW_IR_PASS_CFG_SIMPLIFY,"cfg-simplification"},
    {BREW_IR_PASS_ALGEBRAIC,"algebraic-simplification"},
    {BREW_IR_PASS_COPY_PROPAGATION,"copy-propagation"},
    {BREW_IR_PASS_COMMON_SUBEXPRESSION,"common-subexpression-elimination"},
    {BREW_IR_PASS_LOOP_INVARIANT,"loop-invariant-code-motion"},
    {BREW_IR_PASS_COMMON_SUBEXPRESSION,"post-licm-common-subexpression-elimination"},
    {BREW_IR_PASS_DEAD_CODE,"dead-code-elimination"}
};
static inline bool brew_ir_run_pass_impl(BrewIRModule *module,BrewIRPassKind pass,bool *changed_out,char *error,size_t cap){
    bool changed=false;
    if(changed_out)*changed_out=false;
    if(pass==BREW_IR_PASS_CONSTANT_FOLD){
        for(size_t f=0;f<module->function_count;f++){
            BrewIRFunction *fn=&module->functions[f];
            size_t count=fn->next_value;
            if(count>SIZE_MAX/sizeof(int64_t)||count>SIZE_MAX/sizeof(BrewIRInst*)) {
                if(error&&cap)snprintf(error,cap,"constant-fold analysis size overflow");
                return false;
            }
            unsigned char *known=calloc(count,sizeof(*known));
            int64_t *values=calloc(count,sizeof(*values));
            BrewIRInst **definitions=calloc(count,sizeof(*definitions));
            if(!known||!values||!definitions){
                free(known);free(values);free(definitions);
                if(error&&cap)snprintf(error,cap,"out of memory during constant-fold analysis");
                return false;
            }
            for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
                BrewIRInst *in=&fn->blocks[b].instructions[i];
                if(in->result&&in->result<count)definitions[in->result]=in;
                if(in->op==BREW_IR_CONST&&in->type!=BREW_IR_PTR){
                    unsigned bits=brew_ir_integer_bits(in->type);
                    int64_t normalized=brew_ir_signed_value((uint64_t)in->immediate&brew_ir_width_mask(bits),bits);
                    if(normalized!=in->immediate)changed=true;
                    in->immediate=normalized;
                }
            }
            for(uint32_t id=(uint32_t)fn->parameter_count+1;id<fn->next_value;id++){
                BrewIRInst *in=definitions[id];
                if(!in)continue;
                if(in->op==BREW_IR_CONST){known[id]=1;values[id]=in->immediate;continue;}
                BrewIROp old_op=in->op;
                int64_t old_immediate=in->immediate;
                if(brew_ir_fold_instruction_using(fn,in,known,values)){
                    known[id]=1;values[id]=in->immediate;
                    if(in->op!=old_op||in->immediate!=old_immediate)changed=true;
                }
            }
            free(known);free(values);free(definitions);
        }
        if(changed_out)*changed_out=changed;
        return brew_ir_verify_pass_result(module,error,cap);
    }
    if(pass==BREW_IR_PASS_CFG_SIMPLIFY){if(!brew_ir_cfg_simplify(module,&changed,error,cap))return false;if(changed_out)*changed_out=changed;return brew_ir_verify_pass_result(module,error,cap);}
    if(pass==BREW_IR_PASS_ALGEBRAIC){if(!brew_ir_algebraic_simplify(module,&changed,error,cap))return false;if(changed_out)*changed_out=changed;return brew_ir_verify_pass_result(module,error,cap);}
    if(pass==BREW_IR_PASS_COPY_PROPAGATION){if(!brew_ir_copy_propagation(module,&changed,error,cap))return false;if(changed_out)*changed_out=changed;return brew_ir_verify_pass_result(module,error,cap);}
    if(pass==BREW_IR_PASS_COMMON_SUBEXPRESSION){if(!brew_ir_common_subexpression_eliminate(module,&changed,error,cap))return false;if(changed_out)*changed_out=changed;return brew_ir_verify_pass_result(module,error,cap);}
    if(pass==BREW_IR_PASS_LOOP_INVARIANT){if(!brew_ir_loop_invariant_code_motion(module,&changed,error,cap))return false;if(changed_out)*changed_out=changed;return true;}
    if(pass==BREW_IR_PASS_DEAD_CODE){
        bool changed=false;
        if(!brew_ir_dead_code_eliminate(module,&changed)){if(error&&cap)snprintf(error,cap,"out of memory during dead-code elimination");return false;}
        if(changed_out)*changed_out=changed;
        return brew_ir_verify_pass_result(module,error,cap);
    }
    if(error&&cap)snprintf(error,cap,"unknown IR optimization pass");
    return false;
}
static inline BrewProfileStage brew_ir_pass_profile_stage(BrewIRPassKind pass){
    switch(pass){
        case BREW_IR_PASS_CONSTANT_FOLD:return BREW_PROFILE_IR_PASS_CONSTANT_FOLD;
        case BREW_IR_PASS_CFG_SIMPLIFY:return BREW_PROFILE_IR_PASS_CFG;
        case BREW_IR_PASS_ALGEBRAIC:return BREW_PROFILE_IR_PASS_ALGEBRAIC;
        case BREW_IR_PASS_COPY_PROPAGATION:return BREW_PROFILE_IR_PASS_COPY;
        case BREW_IR_PASS_COMMON_SUBEXPRESSION:return BREW_PROFILE_IR_PASS_CSE;
        case BREW_IR_PASS_LOOP_INVARIANT:return BREW_PROFILE_IR_PASS_LICM;
        case BREW_IR_PASS_DEAD_CODE:return BREW_PROFILE_IR_PASS_DCE;
    }
    return BREW_PROFILE_IR_PASS_DCE;
}
static inline bool brew_ir_run_pass(BrewIRModule *module,BrewIRPassKind pass,bool *changed_out,char *error,size_t cap){
    uint64_t stage=brew_profile_start();
    bool ok=brew_ir_run_pass_impl(module,pass,changed_out,error,cap);
    brew_profile_end(brew_ir_pass_profile_stage(pass),stage);
    return ok;
}
#define BREW_IR_MAX_OPTIMIZATION_PASSES 350u
typedef struct {
    size_t passes_executed;
    size_t rounds;
    bool converged;
} BrewIROptStats;

static inline bool brew_ir_optimize_with_stats(BrewIRModule *module,
    BrewIROptStats *stats,char *error,size_t cap) {
    if(stats)memset(stats,0,sizeof(*stats));
    if(!brew_ir_verify(module,error,cap))return false;
    const size_t pass_count=sizeof(brew_ir_default_passes)/sizeof(brew_ir_default_passes[0]);
    size_t executed=0,rounds=0;
    bool converged=false;
    while(executed<BREW_IR_MAX_OPTIMIZATION_PASSES&&brew_compiler_optimization_passes<BREW_COMPILER_OPTIMIZATION_BUDGET){
        bool round_changed=false;
        rounds++;
        for(size_t i=0;i<pass_count&&executed<BREW_IR_MAX_OPTIMIZATION_PASSES&&brew_compiler_optimization_passes<BREW_COMPILER_OPTIMIZATION_BUDGET;i++,executed++){
            if(!brew_optimizer_consume(1)){if(error&&cap)snprintf(error,cap,"compiler-wide optimization budget exhausted during SVM IR optimization");return false;}
            bool changed=false;
            if(!brew_ir_run_pass(module,brew_ir_default_passes[i].kind,&changed,error,cap)){
                if(stats){stats->passes_executed=executed+1;stats->rounds=rounds;}
                return false;
            }
            round_changed=round_changed||changed;
        }
        if(!round_changed){converged=true;break;}
    }
    if(stats){stats->passes_executed=executed;stats->rounds=rounds;stats->converged=converged;}
    if(brew_profile_enabled){brew_profile_ir_pass_executions+=executed;brew_profile_ir_pass_rounds+=rounds;}
    if(!converged){
        if(error&&cap)snprintf(error,cap,"optimization did not converge within the compiler-wide %u-pass budget",BREW_COMPILER_OPTIMIZATION_BUDGET);
        return false;
    }
    /* Final verifier and derived-analysis rebuild are mandatory. */

    for(size_t function_index=0;function_index<module->function_count;function_index++){
        BrewIRFunction *function=&module->functions[function_index];
        if(function->value_locations&&!brew_ir_rebuild_value_locations(function)){
            if(error&&cap)snprintf(error,cap,
                "failed to rebuild SSA value locations for function '%s'",
                function->name?function->name:"<unnamed>");
            return false;
        }
        for(size_t block_index=0;block_index<function->block_count;block_index++){
            function->blocks[block_index].owner=function;
            function->blocks[block_index].index=(uint32_t)block_index;
        }
    }
    return brew_ir_verify(module,error,cap);
}

static inline bool brew_ir_optimize(BrewIRModule *module,char *error,size_t cap) {
    return brew_ir_optimize_with_stats(module,NULL,error,cap);
}
#endif
