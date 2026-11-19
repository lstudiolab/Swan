#ifndef BREW_IR_H
#define BREW_IR_H
#include "../Basic/Core.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Brew IR is typed, target-independent SSA. IDs are local to a function. */
typedef enum {
 BREW_IR_VOID, BREW_IR_I1, BREW_IR_I8, BREW_IR_I16, BREW_IR_I32,
 BREW_IR_I64, BREW_IR_F32, BREW_IR_F64, BREW_IR_PTR
} BrewIRType;
typedef enum {
 BREW_IR_CONST, BREW_IR_ADD, BREW_IR_SUB, BREW_IR_MUL,
 BREW_IR_SDIV, BREW_IR_UDIV, BREW_IR_SREM, BREW_IR_UREM,
 BREW_IR_AND, BREW_IR_OR, BREW_IR_XOR, BREW_IR_SHL,
 BREW_IR_LSHR, BREW_IR_ASHR, BREW_IR_ICMP_EQ, BREW_IR_ICMP_NE,
 BREW_IR_ICMP_SLT, BREW_IR_ICMP_SLE, BREW_IR_ICMP_SGT,
 BREW_IR_ICMP_SGE, BREW_IR_SELECT, BREW_IR_PHI, BREW_IR_ALLOCA,
 BREW_IR_LOAD, BREW_IR_STORE, BREW_IR_CALL
} BrewIROp;
typedef enum { BREW_IR_BR, BREW_IR_COND_BR, BREW_IR_RET, BREW_IR_UNREACHABLE } BrewIRTermKind;
typedef struct { uint32_t value, block; } BrewIRIncoming;
typedef struct { uint32_t block, index; } BrewIRValueLocation;
typedef struct {
 BrewIROp op; BrewIRType type; uint32_t result, a, b, c;
 int64_t immediate; const char *symbol;
 BrewIRIncoming *incoming; size_t incoming_count;
} BrewIRInst;
typedef struct {
 BrewIRTermKind kind; uint32_t value, target, alternate;
} BrewIRTerm;
typedef struct BrewIRFunction BrewIRFunction;
typedef struct {
 const char *name; BrewIRInst *instructions; size_t instruction_count, instruction_capacity;
 BrewIRTerm terminator; bool terminated; BrewIRFunction *owner; uint32_t index;
} BrewIRBlock;
struct BrewIRFunction {
 const char *name; BrewIRType return_type; BrewIRType *parameter_types;
 size_t parameter_count; bool external,exported; BrewIRBlock *blocks; size_t block_count, block_capacity;
 uint32_t next_value; BrewIRType *value_types; size_t value_type_capacity;
 BrewIRValueLocation *value_locations; size_t value_location_capacity;
};
typedef struct { BrewIRFunction *functions; size_t function_count, function_capacity; char **owned_strings; size_t owned_string_count, owned_string_capacity; bool allow_no_main; } BrewIRModule;

static inline const char *brew_ir_type_name(BrewIRType t) {
 static const char *names[]={"void","i1","i8","i16","i32","i64","f32","f64","ptr"};
 return (unsigned)t<sizeof(names)/sizeof(names[0])?names[t]:"invalid";
}
static inline bool brew_ir_type_valid(BrewIRType t) { return (unsigned)t<=BREW_IR_PTR; }
static inline bool brew_ir_value_type(BrewIRType t) { return brew_ir_type_valid(t)&&t!=BREW_IR_VOID; }
static inline bool brew_ir_reserve(void **p,size_t *capacity,size_t need,size_t item_size) {
 if(!p||!capacity||!item_size||*capacity>SIZE_MAX/item_size||need>SIZE_MAX/item_size)return false;
 if(need<=*capacity)return true;
 size_t n=*capacity?*capacity:8;
 while(n<need){if(n>SIZE_MAX/2){n=need;break;}n*=2;}
 if(n>SIZE_MAX/item_size)return false;
 void *q=realloc(*p,n*item_size);if(!q)return false;*p=q;*capacity=n;return true;
}
static inline bool brew_ir_own_string(BrewIRModule *m,char *value){
 if(!m||!value||m->owned_string_count==SIZE_MAX)return false;
 if(!brew_ir_reserve((void**)&m->owned_strings,&m->owned_string_capacity,m->owned_string_count+1,sizeof(*m->owned_strings)))return false;
 m->owned_strings[m->owned_string_count++]=value;return true;
}
static inline void brew_ir_destroy(BrewIRModule *m) {
 if(!m)return;
 for(size_t f=0;f<m->function_count;f++){
  BrewIRFunction *fn=&m->functions[f];
  for(size_t b=0;b<fn->block_count;b++){
   BrewIRBlock *bb=&fn->blocks[b];
   for(size_t i=0;i<bb->instruction_count;i++)free(bb->instructions[i].incoming);
   free(bb->instructions);
  }
  free(fn->blocks);free(fn->parameter_types);free(fn->value_types);free(fn->value_locations);
 }
 free(m->functions);for(size_t i=0;i<m->owned_string_count;i++)free(m->owned_strings[i]);free(m->owned_strings);memset(m,0,sizeof(*m));
}
static inline BrewIRFunction *brew_ir_add_function(BrewIRModule *m,const char *name,BrewIRType ret,const BrewIRType *params,size_t count) {
 if(!m||!name||!name[0]||!brew_ir_type_valid(ret)||(count&&!params)||count>=(size_t)(INT32_MAX/8)||count>SIZE_MAX/sizeof(*params)||m->function_count==SIZE_MAX)return NULL;
 for(size_t i=0;i<count;i++)if(!brew_ir_value_type(params[i]))return NULL;
 BrewIRFunction *previous_functions=m->functions;
 if(!brew_ir_reserve((void**)&m->functions,&m->function_capacity,m->function_count+1,sizeof(*m->functions)))return NULL;
 if(m->functions!=previous_functions){
  for(size_t function_index=0;function_index<m->function_count;function_index++){
   BrewIRFunction *existing=&m->functions[function_index];
   for(size_t block_index=0;block_index<existing->block_count;block_index++)
    existing->blocks[block_index].owner=existing;
  }
 }
 BrewIRFunction *f=&m->functions[m->function_count++];memset(f,0,sizeof(*f));f->name=name;f->return_type=ret;
 if(count){f->parameter_types=malloc(count*sizeof(*params));if(!f->parameter_types){m->function_count--;return NULL;}memcpy(f->parameter_types,params,count*sizeof(*params));}
 f->value_type_capacity=count+1;f->value_types=calloc(f->value_type_capacity,sizeof(*f->value_types));
 f->value_location_capacity=count+1;f->value_locations=calloc(f->value_location_capacity,sizeof(*f->value_locations));
 if(!f->value_types||!f->value_locations){free(f->parameter_types);free(f->value_types);free(f->value_locations);m->function_count--;return NULL;}
 for(size_t i=0;i<f->value_location_capacity;i++)f->value_locations[i]=(BrewIRValueLocation){UINT32_MAX,UINT32_MAX};
 for(size_t i=0;i<count;i++)f->value_types[i+1]=params[i];
 f->parameter_count=count;f->next_value=(uint32_t)count+1;return f;
}
static inline BrewIRBlock *brew_ir_add_block(BrewIRFunction *f,const char *name) {
 if(!f||f->block_count>=UINT32_MAX||f->block_count==SIZE_MAX||!brew_ir_reserve((void**)&f->blocks,&f->block_capacity,f->block_count+1,sizeof(*f->blocks)))return NULL;
 BrewIRBlock *b=&f->blocks[f->block_count++];memset(b,0,sizeof(*b));b->name=name;b->owner=f;b->index=(uint32_t)(f->block_count-1);return b;
}
static inline uint32_t brew_ir_add_inst(BrewIRFunction *f,BrewIRBlock *b,BrewIRInst inst) {
 if(!f||!b||b->owner!=f||b->index>=f->block_count||&f->blocks[b->index]!=b||b->terminated||(inst.incoming_count&&!inst.incoming)||b->instruction_count==SIZE_MAX||!brew_ir_reserve((void**)&b->instructions,&b->instruction_capacity,b->instruction_count+1,sizeof(*b->instructions)))return 0;
 if(inst.type==BREW_IR_VOID)inst.result=0;else{
  if(!brew_ir_value_type(inst.type)||f->next_value==0||f->next_value>=(uint32_t)(INT32_MAX/8)||
     !brew_ir_reserve((void**)&f->value_types,&f->value_type_capacity,(size_t)f->next_value+1,sizeof(*f->value_types))||
     !brew_ir_reserve((void**)&f->value_locations,&f->value_location_capacity,(size_t)f->next_value+1,sizeof(*f->value_locations)))return 0;
  inst.result=f->next_value++;f->value_types[inst.result]=inst.type;
  f->value_locations[inst.result]=(BrewIRValueLocation){b->index,(uint32_t)b->instruction_count};
 }
 b->instructions[b->instruction_count++]=inst;return inst.result;
}
static inline bool brew_ir_rebuild_value_locations(BrewIRFunction *fn){
 if(!fn||!fn->value_locations||fn->value_location_capacity<fn->next_value)return false;
 for(size_t id=0;id<fn->next_value;id++)
  fn->value_locations[id]=(BrewIRValueLocation){UINT32_MAX,UINT32_MAX};
 for(size_t b=0;b<fn->block_count;b++){
  if(b>UINT32_MAX)return false;
  for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
   uint32_t id=fn->blocks[b].instructions[i].result;
   if(id){
    if(id>=fn->next_value||i>UINT32_MAX)return false;
    fn->value_locations[id]=(BrewIRValueLocation){(uint32_t)b,(uint32_t)i};
   }
  }
 }
 return true;
}
static inline bool brew_ir_terminate(BrewIRBlock *b,BrewIRTerm term) {
 if(!b||b->terminated)return false;
 b->terminator=term;
 b->terminated=true;
 return true;
}
static inline bool brew_ir_fail(char *error,size_t cap,const char *message,const char *fn,size_t block,size_t inst) {
 if(error&&cap)snprintf(error,cap,"%s in %s, block %zu, instruction %zu",message,fn?fn:"<module>",block,inst);
 return false;
}
static inline BrewIRType brew_ir_value_type_of(const BrewIRFunction *f,uint32_t id) {
 if(!f||!id||id>=f->next_value)return BREW_IR_VOID;
 if(id<=f->parameter_count)return f->parameter_types[id-1];
 if(f->value_types&&id<f->value_type_capacity)return f->value_types[id];
 for(size_t b=0;b<f->block_count;b++)for(size_t i=0;i<f->blocks[b].instruction_count;i++)
  if(f->blocks[b].instructions[i].result==id)return f->blocks[b].instructions[i].type;
 return BREW_IR_VOID;
}
/* A definition dominates a use when every path from entry to the use
 * passes through the definition block. This path test naturally supports
 * forward-numbered blocks and loop backedges without relying on block order. */
static inline bool brew_ir_block_dominates(const BrewIRFunction *f,size_t def_block,size_t use_block) {
 size_t n=f->block_count;
 if(def_block>=n||use_block>=n||n>SIZE_MAX/sizeof(size_t))return false;
 if(def_block==use_block)return true;
 bool *seen=calloc(n,sizeof(*seen));
 size_t *queue=malloc(n*sizeof(*queue));
 if(!seen||!queue){free(seen);free(queue);return false;}
 size_t head=0,tail=0;
 seen[0]=true;queue[tail++]=0;
 bool reachable=false;
 while(head<tail){
  size_t b=queue[head++];
  if(b==use_block){reachable=true;break;}
  const BrewIRTerm *t=&f->blocks[b].terminator;
  size_t targets[2],count=0;
  if(t->kind==BREW_IR_BR)targets[count++]=t->target;
  else if(t->kind==BREW_IR_COND_BR){targets[count++]=t->target;targets[count++]=t->alternate;}
  for(size_t k=0;k<count;k++)if(targets[k]<n&&!seen[targets[k]]){seen[targets[k]]=true;queue[tail++]=targets[k];}
 }
 if(!reachable){free(seen);free(queue);return false;}
 memset(seen,0,n*sizeof(*seen));head=tail=0;
 seen[0]=true;queue[tail++]=0;
 bool bypass=false;
 while(head<tail){
  size_t b=queue[head++];
  if(b==def_block)continue;
  if(b==use_block){bypass=true;break;}
  const BrewIRTerm *t=&f->blocks[b].terminator;
  size_t targets[2],count=0;
  if(t->kind==BREW_IR_BR)targets[count++]=t->target;
  else if(t->kind==BREW_IR_COND_BR){targets[count++]=t->target;targets[count++]=t->alternate;}
  for(size_t k=0;k<count;k++)if(targets[k]<n&&!seen[targets[k]]){seen[targets[k]]=true;queue[tail++]=targets[k];}
 }
 free(seen);free(queue);
 return !bypass;
}
static inline bool brew_ir_verify_use(const BrewIRFunction *function,
                                           uint32_t value_id,
                                           size_t use_block,
                                           size_t use_instruction,
                                           char *error,
                                           size_t capacity)
{
    if (value_id == 0) {
        return brew_ir_fail(error, capacity, "undefined value",
                            function->name, use_block, use_instruction);
    }

    if (value_id <= function->parameter_count) {
        return true;
    }

    /*
     * Builders maintain this location index as values are emitted. Use it as
     * the fast path, but validate the entry before trusting it: optimization
     * passes may move instructions and leave the index stale until rebuilt.
     */
    if (function->value_locations != NULL &&
        value_id < function->value_location_capacity) {
        BrewIRValueLocation location = function->value_locations[value_id];

        if (location.block < function->block_count) {
            const BrewIRBlock *definition_block =
                &function->blocks[location.block];

            if (location.index < definition_block->instruction_count &&
                definition_block->instructions[location.index].result == value_id) {
                if (location.block == use_block) {
                    if (location.index >= use_instruction) {
                        return brew_ir_fail(error, capacity,
                                            "value used before its definition",
                                            function->name, use_block,
                                            use_instruction);
                    }

                    return true;
                }

                if (!brew_ir_block_dominates(function, location.block, use_block)) {
                    return brew_ir_fail(error, capacity,
                                        "definition does not dominate use",
                                        function->name, use_block,
                                        use_instruction);
                }

                return true;
            }
        }
    }

    /* Conservative fallback for IR whose location index is absent or stale. */
    for (size_t block_index = 0;
         block_index < function->block_count;
         ++block_index) {
        const BrewIRBlock *block = &function->blocks[block_index];

        for (size_t instruction_index = 0;
             instruction_index < block->instruction_count;
             ++instruction_index) {
            if (block->instructions[instruction_index].result != value_id) {
                continue;
            }

            if (block_index == use_block) {
                if (instruction_index >= use_instruction) {
                    return brew_ir_fail(error, capacity,
                                        "value used before its definition",
                                        function->name, use_block,
                                        use_instruction);
                }

                return true;
            }

            if (!brew_ir_block_dominates(function, block_index, use_block)) {
                return brew_ir_fail(error, capacity,
                                    "definition does not dominate use",
                                    function->name, use_block,
                                    use_instruction);
            }

            return true;
        }
    }

    return brew_ir_fail(error, capacity, "use of undefined value",
                        function->name, use_block, use_instruction);
}

static inline bool brew_ir_integer_type(BrewIRType t) {
 return t>=BREW_IR_I1&&t<=BREW_IR_I64;
}
typedef struct {
    uint32_t value;
    uint32_t block;
    uint32_t instruction;
} BrewIRDefinition;

static inline int brew_ir_compare_definitions(const void *lhs, const void *rhs)
{
    const BrewIRDefinition *left = lhs;
    const BrewIRDefinition *right = rhs;

    if (left->value != right->value) {
        return (left->value > right->value) - (left->value < right->value);
    }

    if (left->block != right->block) {
        return (left->block > right->block) - (left->block < right->block);
    }

    return (left->instruction > right->instruction) -
           (left->instruction < right->instruction);
}

/*
 * Validate SSA definition uniqueness in O(I log I), rather than repeatedly
 * rescanning all preceding instructions for every definition. The compact
 * sorted index preserves the exact location of a malformed redefinition.
 */
static inline bool brew_ir_verify_unique_definitions(const BrewIRFunction *fn,
                                                      char *error,
                                                      size_t capacity)
{
    size_t definition_count = 0;

    for (size_t block_index = 0; block_index < fn->block_count; ++block_index) {
        const BrewIRBlock *block = &fn->blocks[block_index];

        if (block->instruction_count > block->instruction_capacity ||
            (block->instruction_count != 0 && block->instructions == NULL)) {
            return brew_ir_fail(error, capacity,
                                "invalid block instruction storage",
                                fn->name, block_index, 0);
        }

        if (block->instruction_count > SIZE_MAX - definition_count) {
            return brew_ir_fail(error, capacity,
                                "SSA definition count overflow",
                                fn->name, block_index, 0);
        }

        for (size_t instruction_index = 0;
             instruction_index < block->instruction_count;
             ++instruction_index) {
            if (block->instructions[instruction_index].result != 0) {
                ++definition_count;
            }
        }
    }

    if (definition_count > SIZE_MAX / sizeof(BrewIRDefinition)) {
        return brew_ir_fail(error, capacity,
                            "SSA definition table size overflow",
                            fn->name, 0, 0);
    }

    BrewIRDefinition *definitions = NULL;
    if (definition_count != 0) {
        definitions = malloc(definition_count * sizeof(*definitions));
        if (definitions == NULL) {
            return brew_ir_fail(error, capacity,
                                "out of memory validating SSA definitions",
                                fn->name, 0, 0);
        }
    }

    size_t next_definition = 0;
    for (size_t block_index = 0; block_index < fn->block_count; ++block_index) {
        const BrewIRBlock *block = &fn->blocks[block_index];

        for (size_t instruction_index = 0;
             instruction_index < block->instruction_count;
             ++instruction_index) {
            uint32_t result = block->instructions[instruction_index].result;
            if (result == 0) {
                continue;
            }

            if (block_index > UINT32_MAX || instruction_index > UINT32_MAX) {
                free(definitions);
                return brew_ir_fail(error, capacity,
                                    "SSA definition location exceeds IR index range",
                                    fn->name, block_index, instruction_index);
            }

            definitions[next_definition++] = (BrewIRDefinition) {
                .value = result,
                .block = (uint32_t)block_index,
                .instruction = (uint32_t)instruction_index
            };
        }
    }

    if (definition_count > 1) {
        qsort(definitions, definition_count, sizeof(*definitions),
              brew_ir_compare_definitions);
    }

    for (size_t index = 1; index < definition_count; ++index) {
        if (definitions[index - 1].value == definitions[index].value) {
            BrewIRDefinition duplicate = definitions[index];
            free(definitions);

            return brew_ir_fail(error, capacity, "duplicate SSA value ID",
                                fn->name, duplicate.block, duplicate.instruction);
        }
    }

    free(definitions);
    return true;
}

static inline int brew_ir_compare_function_names(const void *left,const void *right){
 const BrewIRFunction *a=*(const BrewIRFunction *const*)left;
 const BrewIRFunction *b=*(const BrewIRFunction *const*)right;
 if(!a->name)return b->name?1:0;
 if(!b->name)return -1;
 return strcmp(a->name,b->name);
}
static inline const BrewIRFunction *brew_ir_find_function_by_name(
    const BrewIRFunction *const *index,size_t count,const char *name){
 size_t low=0,high=count;
 while(low<high){
  size_t middle=low+(high-low)/2;
  const BrewIRFunction *candidate=index[middle];
  if(!candidate->name){high=middle;continue;}
  int comparison=strcmp(candidate->name,name);
  if(comparison<0)low=middle+1;
  else if(comparison>0)high=middle;
  else return candidate;
 }
 return NULL;
}
static inline bool brew_ir_verify_body(const BrewIRModule *m,
    const BrewIRFunction *const *function_index,size_t function_index_count,
    char *error,size_t cap) {
 if(!m||m->function_count>m->function_capacity||(m->function_count&&!m->functions))return brew_ir_fail(error,cap,"invalid module storage",NULL,0,0);
 for(size_t f=0;f<m->function_count;f++){
  const BrewIRFunction *fn=&m->functions[f];
  if(!fn->name||!fn->name[0]||!brew_ir_type_valid(fn->return_type)||fn->next_value<=fn->parameter_count||fn->next_value>(uint32_t)(INT32_MAX/8))return brew_ir_fail(error,cap,"invalid function signature or register range",fn->name,0,0);
  if(fn->parameter_count>UINT32_MAX-1u||(fn->parameter_count&&!fn->parameter_types)||fn->block_count>fn->block_capacity||(fn->block_count&&!fn->blocks))return brew_ir_fail(error,cap,"invalid function storage or parameter table",fn->name,0,0);
  if(fn->parameter_count&&!fn->parameter_types)return brew_ir_fail(error,cap,"missing parameter types",fn->name,0,0);
  for(size_t p=0;p<fn->parameter_count;p++)if(!brew_ir_value_type(fn->parameter_types[p]))return brew_ir_fail(error,cap,"invalid parameter type",fn->name,0,p);
  if(fn->external){if(fn->block_count)return brew_ir_fail(error,cap,"external function must not contain a body",fn->name,0,0);continue;}
  if(!fn->block_count)return brew_ir_fail(error,cap,"function has no entry block",fn->name,0,0);
  if(!brew_ir_verify_unique_definitions(fn,error,cap))return false;
  for(size_t b=0;b<fn->block_count;b++){
   const BrewIRBlock *bb=&fn->blocks[b];
   if(bb->owner!=fn||bb->index!=b)
    return brew_ir_fail(error,cap,"invalid block ownership or index metadata",fn->name,b,0);
   if(bb->instruction_count>bb->instruction_capacity||(bb->instruction_count&&!bb->instructions))return brew_ir_fail(error,cap,"invalid block instruction storage",fn->name,b,0);
   if(!bb->terminated)return brew_ir_fail(error,cap,"block has no terminator",fn->name,b,bb->instruction_count);
   bool saw_non_phi=false;
   for(size_t i=0;i<bb->instruction_count;i++){
    const BrewIRInst *in=&bb->instructions[i];
    if((unsigned)in->op>BREW_IR_CALL||!brew_ir_type_valid(in->type)||(in->type==BREW_IR_VOID?in->result!=0:in->result==0))return brew_ir_fail(error,cap,"invalid instruction opcode, result type, or ID",fn->name,b,i);
    if(in->op==BREW_IR_PHI){if(saw_non_phi)return brew_ir_fail(error,cap,"phi instructions must precede non-phi instructions",fn->name,b,i);}
    else saw_non_phi=true;
    if(in->result){
     if(in->result>=fn->next_value)return brew_ir_fail(error,cap,"SSA value ID exceeds function register range",fn->name,b,i);
     if(in->result<=fn->parameter_count)return brew_ir_fail(error,cap,"instruction redefines a parameter",fn->name,b,i);
    }
    if(in->op==BREW_IR_CONST){
     if(!brew_ir_value_type(in->type))
      return brew_ir_fail(error,cap,"constant cannot have void type",fn->name,b,i);
     if((in->type==BREW_IR_I1&&(in->immediate<0||in->immediate>1))||
        (in->type==BREW_IR_I8&&(in->immediate<INT8_MIN||in->immediate>INT8_MAX))||
        (in->type==BREW_IR_I16&&(in->immediate<INT16_MIN||in->immediate>INT16_MAX))||
        (in->type==BREW_IR_I32&&(in->immediate<INT32_MIN||in->immediate>INT32_MAX)))
      return brew_ir_fail(error,cap,"integer constant is outside its declared type range",fn->name,b,i);
    }
    else if(in->op==BREW_IR_PHI){
     if(!in->incoming_count||!in->incoming)return brew_ir_fail(error,cap,"phi has no incoming edges",fn->name,b,i);
     for(size_t k=0;k<in->incoming_count;k++){
      BrewIRIncoming edge=in->incoming[k];
      if(edge.block>=fn->block_count||!brew_ir_verify_use(fn,edge.value,edge.block,fn->blocks[edge.block].instruction_count,error,cap))return false;
      const BrewIRTerm *pred_term=&fn->blocks[edge.block].terminator;
      bool is_pred=(pred_term->kind==BREW_IR_BR&&pred_term->target==b)||(pred_term->kind==BREW_IR_COND_BR&&(pred_term->target==b||pred_term->alternate==b));
      if(!is_pred)return brew_ir_fail(error,cap,"phi incoming block is not a predecessor",fn->name,b,i);
      if(brew_ir_value_type_of(fn,edge.value)!=in->type)return brew_ir_fail(error,cap,"phi incoming type mismatch",fn->name,b,i);
      for(size_t j=0;j<k;j++)if(in->incoming[j].block==edge.block)return brew_ir_fail(error,cap,"phi has duplicate predecessor",fn->name,b,i);
     }
     size_t predecessor_count=0;
     for(size_t pb=0;pb<fn->block_count;pb++){
      const BrewIRTerm *pt=&fn->blocks[pb].terminator;
      if((pt->kind==BREW_IR_BR&&pt->target==b)||(pt->kind==BREW_IR_COND_BR&&(pt->target==b||pt->alternate==b)))predecessor_count++;
     }
     if(predecessor_count!=in->incoming_count)return brew_ir_fail(error,cap,"phi must define one incoming value for every predecessor",fn->name,b,i);
    }else if(in->op==BREW_IR_ALLOCA){if(in->type!=BREW_IR_PTR)return brew_ir_fail(error,cap,"alloca must produce ptr",fn->name,b,i);}
    else if(in->op==BREW_IR_CALL){
     if(!in->symbol||(in->incoming_count&&!in->incoming))return brew_ir_fail(error,cap,"call has no callee symbol or argument array",fn->name,b,i);
     const BrewIRFunction *callee=brew_ir_find_function_by_name(function_index,function_index_count,in->symbol);
     if(!callee)return brew_ir_fail(error,cap,"call references an unknown function",fn->name,b,i);
     if(!brew_ir_type_valid(callee->return_type)||(callee->parameter_count&&!callee->parameter_types))return brew_ir_fail(error,cap,"callee has an invalid signature",fn->name,b,i);
     if(callee->return_type!=in->type||callee->parameter_count!=in->incoming_count)return brew_ir_fail(error,cap,"call signature does not match callee",fn->name,b,i);
     for(size_t ai=0;ai<in->incoming_count;ai++){
      uint32_t id=in->incoming[ai].value;
      if(!brew_ir_verify_use(fn,id,b,i,error,cap))return false;
      if(brew_ir_value_type_of(fn,id)!=callee->parameter_types[ai])return brew_ir_fail(error,cap,"call argument type mismatch",fn->name,b,i);
     }
    }
    else if(in->op==BREW_IR_LOAD||in->op==BREW_IR_STORE){
     if(!brew_ir_verify_use(fn,in->a,b,i,error,cap)||brew_ir_value_type_of(fn,in->a)!=BREW_IR_PTR)return brew_ir_fail(error,cap,"memory operation requires a defined pointer",fn->name,b,i);
     if(in->op==BREW_IR_LOAD&&!brew_ir_value_type(in->type))return brew_ir_fail(error,cap,"load must produce a value",fn->name,b,i);
     if(in->op==BREW_IR_STORE&&(in->type!=BREW_IR_VOID||!brew_ir_verify_use(fn,in->b,b,i,error,cap)))return brew_ir_fail(error,cap,"store requires void result and a defined value",fn->name,b,i);
    }else if(in->op==BREW_IR_SELECT){
     if(!brew_ir_verify_use(fn,in->a,b,i,error,cap)||!brew_ir_verify_use(fn,in->b,b,i,error,cap)||!brew_ir_verify_use(fn,in->c,b,i,error,cap))return false;
     if(brew_ir_value_type_of(fn,in->a)!=BREW_IR_I1||brew_ir_value_type_of(fn,in->b)!=in->type||brew_ir_value_type_of(fn,in->c)!=in->type)return brew_ir_fail(error,cap,"select operand type mismatch",fn->name,b,i);
    }else if(in->op!=BREW_IR_PHI&&in->op!=BREW_IR_ALLOCA&&in->op!=BREW_IR_CALL&&in->op!=BREW_IR_LOAD&&in->op!=BREW_IR_STORE){
     if(!brew_ir_verify_use(fn,in->a,b,i,error,cap)||!brew_ir_verify_use(fn,in->b,b,i,error,cap))return false;
     BrewIRType a=brew_ir_value_type_of(fn,in->a),c=brew_ir_value_type_of(fn,in->b);
     bool cmp=in->op>=BREW_IR_ICMP_EQ&&in->op<=BREW_IR_ICMP_SGE;
     if(!brew_ir_integer_type(a)||a!=c||(cmp?in->type!=BREW_IR_I1:a!=in->type))return brew_ir_fail(error,cap,"binary operand type mismatch: operation requires matching integer operands",fn->name,b,i);
    }
    if(in->op==BREW_IR_CONST){
     if(in->type==BREW_IR_PTR||in->a||in->b||in->c||in->symbol||in->incoming_count)return brew_ir_fail(error,cap,"constant has an invalid type or unused operands",fn->name,b,i);
    }else if(in->op==BREW_IR_ALLOCA){
     if(in->type!=BREW_IR_PTR||in->a||in->b||in->c||in->symbol||in->incoming_count)return brew_ir_fail(error,cap,"alloca has an invalid result or unused operands",fn->name,b,i);
    }else if(in->op==BREW_IR_LOAD){
     if(!brew_ir_value_type(in->type)||in->b||in->c||in->symbol||in->incoming_count)return brew_ir_fail(error,cap,"load has an invalid result or unused operands",fn->name,b,i);
    }else if(in->op==BREW_IR_STORE){
     if(in->type!=BREW_IR_VOID||in->c||in->symbol||in->incoming_count)return brew_ir_fail(error,cap,"store has an invalid result or unused operands",fn->name,b,i);
    }else if(in->op==BREW_IR_CALL){
     if(in->a||in->b||in->c)return brew_ir_fail(error,cap,"call has unexpected scalar operands",fn->name,b,i);
    }else if(in->op==BREW_IR_PHI){
     if(!brew_ir_value_type(in->type)||in->a||in->b||in->c||in->symbol)return brew_ir_fail(error,cap,"phi has an invalid result or unused operands",fn->name,b,i);
    }else if(in->op==BREW_IR_SELECT){
     if(!brew_ir_value_type(in->type)||in->symbol||in->incoming_count)return brew_ir_fail(error,cap,"select has invalid metadata",fn->name,b,i);
    }else if(in->symbol||in->incoming_count||in->a==0||in->b==0||in->c){
     return brew_ir_fail(error,cap,"binary instruction has invalid operands or metadata",fn->name,b,i);
    }
   }
   const BrewIRTerm *t=&bb->terminator;
   if((unsigned)t->kind>BREW_IR_UNREACHABLE)return brew_ir_fail(error,cap,"invalid terminator",fn->name,b,bb->instruction_count);
   if(t->kind==BREW_IR_BR&&(t->value||t->alternate))return brew_ir_fail(error,cap,"unconditional branch has unused operands",fn->name,b,bb->instruction_count);
   if(t->kind==BREW_IR_COND_BR&&!t->value)return brew_ir_fail(error,cap,"conditional branch is missing its condition",fn->name,b,bb->instruction_count);
   if(t->kind==BREW_IR_RET&&(t->target||t->alternate))return brew_ir_fail(error,cap,"return terminator has branch targets",fn->name,b,bb->instruction_count);
   if(t->kind==BREW_IR_UNREACHABLE&&(t->value||t->target||t->alternate))return brew_ir_fail(error,cap,"unreachable terminator has unused operands",fn->name,b,bb->instruction_count);
   if((t->kind==BREW_IR_BR||t->kind==BREW_IR_COND_BR)&&(t->target>=fn->block_count||(t->kind==BREW_IR_COND_BR&&t->alternate>=fn->block_count)))return brew_ir_fail(error,cap,"branch target is outside function",fn->name,b,bb->instruction_count);
   if(t->kind==BREW_IR_COND_BR&&(!brew_ir_verify_use(fn,t->value,b,bb->instruction_count,error,cap)||brew_ir_value_type_of(fn,t->value)!=BREW_IR_I1))return brew_ir_fail(error,cap,"conditional branch requires i1 condition",fn->name,b,bb->instruction_count);
   if(t->kind==BREW_IR_RET){
    if(fn->return_type==BREW_IR_VOID){if(t->value)return brew_ir_fail(error,cap,"void function returns a value",fn->name,b,bb->instruction_count);}
    else {
     if(!t->value)return brew_ir_fail(error,cap,"return value is missing",fn->name,b,bb->instruction_count);
     if(!brew_ir_verify_use(fn,t->value,b,bb->instruction_count,error,cap))return false;
     if(brew_ir_value_type_of(fn,t->value)!=fn->return_type)return brew_ir_fail(error,cap,"return type mismatch",fn->name,b,bb->instruction_count);
    }
   }
  }
 }
 if(error&&cap)error[0]=0;
 return true;
}

static inline bool brew_ir_verify_impl(const BrewIRModule *module,char *error,size_t capacity) {
 if(!module||module->function_count>module->function_capacity||
    (module->function_count&&!module->functions))
  return brew_ir_fail(error,capacity,"invalid module storage",NULL,0,0);
 if(module->function_count>SIZE_MAX/sizeof(const BrewIRFunction*))
  return brew_ir_fail(error,capacity,"function index size overflow",NULL,0,0);
 const BrewIRFunction **function_index=NULL;
 if(module->function_count){
  function_index=malloc(module->function_count*sizeof(*function_index));
  if(!function_index)
   return brew_ir_fail(error,capacity,"out of memory building IR function index",NULL,0,0);
  for(size_t i=0;i<module->function_count;i++)function_index[i]=&module->functions[i];
  qsort(function_index,module->function_count,sizeof(*function_index),brew_ir_compare_function_names);
  for(size_t i=1;i<module->function_count;i++){
   const BrewIRFunction *previous=function_index[i-1];
   const BrewIRFunction *current=function_index[i];
   if(previous->name&&current->name&&!strcmp(previous->name,current->name)){
    const char *duplicate=current->name;
    free(function_index);
    return brew_ir_fail(error,capacity,"duplicate function name",duplicate,0,0);
   }
  }
 }
 bool valid=brew_ir_verify_body(module,function_index,module->function_count,error,capacity);
 free(function_index);
 return valid;
}

static inline bool brew_ir_verify(const BrewIRModule *module,char *error,size_t capacity) {
 uint64_t stage=brew_profile_start();
 bool valid=brew_ir_verify_impl(module,error,capacity);
 brew_profile_end(BREW_PROFILE_IR_VERIFY,stage);
 return valid;
}
#endif
