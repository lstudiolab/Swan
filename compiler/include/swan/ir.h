#ifndef SWAN_IR_H
#define SWAN_IR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Swan IR is typed, target-independent SSA. IDs are local to a function. */
typedef enum {
 SWAN_IR_VOID, SWAN_IR_I1, SWAN_IR_I8, SWAN_IR_I16, SWAN_IR_I32,
 SWAN_IR_I64, SWAN_IR_F32, SWAN_IR_F64, SWAN_IR_PTR
} SwanIRType;
typedef enum {
 SWAN_IR_CONST, SWAN_IR_ADD, SWAN_IR_SUB, SWAN_IR_MUL,
 SWAN_IR_SDIV, SWAN_IR_UDIV, SWAN_IR_SREM, SWAN_IR_UREM,
 SWAN_IR_AND, SWAN_IR_OR, SWAN_IR_XOR, SWAN_IR_SHL,
 SWAN_IR_LSHR, SWAN_IR_ASHR, SWAN_IR_ICMP_EQ, SWAN_IR_ICMP_NE,
 SWAN_IR_ICMP_SLT, SWAN_IR_ICMP_SLE, SWAN_IR_ICMP_SGT,
 SWAN_IR_ICMP_SGE, SWAN_IR_SELECT, SWAN_IR_PHI, SWAN_IR_ALLOCA,
 SWAN_IR_LOAD, SWAN_IR_STORE, SWAN_IR_CALL
} SwanIROp;
typedef enum { SWAN_IR_BR, SWAN_IR_COND_BR, SWAN_IR_RET, SWAN_IR_UNREACHABLE } SwanIRTermKind;
typedef struct { uint32_t value, block; } SwanIRIncoming;
typedef struct {
 SwanIROp op; SwanIRType type; uint32_t result, a, b, c;
 int64_t immediate; const char *symbol;
 SwanIRIncoming *incoming; size_t incoming_count;
} SwanIRInst;
typedef struct {
 SwanIRTermKind kind; uint32_t value, target, alternate;
} SwanIRTerm;
typedef struct {
 const char *name; SwanIRInst *instructions; size_t instruction_count, instruction_capacity;
 SwanIRTerm terminator; bool terminated;
} SwanIRBlock;
typedef struct {
 const char *name; SwanIRType return_type; SwanIRType *parameter_types;
 size_t parameter_count; SwanIRBlock *blocks; size_t block_count, block_capacity;
 uint32_t next_value;
} SwanIRFunction;
typedef struct { SwanIRFunction *functions; size_t function_count, function_capacity; char **owned_strings; size_t owned_string_count, owned_string_capacity; } SwanIRModule;

static inline const char *swan_ir_type_name(SwanIRType t) {
 static const char *names[]={"void","i1","i8","i16","i32","i64","f32","f64","ptr"};
 return (unsigned)t<sizeof(names)/sizeof(names[0])?names[t]:"invalid";
}
static inline bool swan_ir_type_valid(SwanIRType t) { return (unsigned)t<=SWAN_IR_PTR; }
static inline bool swan_ir_value_type(SwanIRType t) { return swan_ir_type_valid(t)&&t!=SWAN_IR_VOID; }
static inline bool swan_ir_reserve(void **p,size_t *capacity,size_t need,size_t item_size) {
 if(need<=*capacity)return true;
 size_t n=*capacity?*capacity:8;
 while(n<need){if(n>SIZE_MAX/2)return false;n*=2;}
 if(n>SIZE_MAX/item_size)return false;
 void *q=realloc(*p,n*item_size);if(!q)return false;*p=q;*capacity=n;return true;
}
static inline bool swan_ir_own_string(SwanIRModule *m,char *value){
 if(!value)return false;
 if(!swan_ir_reserve((void**)&m->owned_strings,&m->owned_string_capacity,m->owned_string_count+1,sizeof(*m->owned_strings)))return false;
 m->owned_strings[m->owned_string_count++]=value;return true;
}
static inline void swan_ir_destroy(SwanIRModule *m) {
 for(size_t f=0;f<m->function_count;f++){
  SwanIRFunction *fn=&m->functions[f];
  for(size_t b=0;b<fn->block_count;b++){
   SwanIRBlock *bb=&fn->blocks[b];
   for(size_t i=0;i<bb->instruction_count;i++)free(bb->instructions[i].incoming);
   free(bb->instructions);
  }
  free(fn->blocks);free(fn->parameter_types);
 }
 free(m->functions);for(size_t i=0;i<m->owned_string_count;i++)free(m->owned_strings[i]);free(m->owned_strings);memset(m,0,sizeof(*m));
}
static inline SwanIRFunction *swan_ir_add_function(SwanIRModule *m,const char *name,SwanIRType ret,const SwanIRType *params,size_t count) {
 if(!name||!swan_ir_type_valid(ret)||!swan_ir_reserve((void**)&m->functions,&m->function_capacity,m->function_count+1,sizeof(*m->functions)))return NULL;
 SwanIRFunction *f=&m->functions[m->function_count++];memset(f,0,sizeof(*f));f->name=name;f->return_type=ret;
 if(count){f->parameter_types=malloc(count*sizeof(*params));if(!f->parameter_types){m->function_count--;return NULL;}memcpy(f->parameter_types,params,count*sizeof(*params));}
 f->parameter_count=count;f->next_value=(uint32_t)count+1;return f;
}
static inline SwanIRBlock *swan_ir_add_block(SwanIRFunction *f,const char *name) {
 if(!swan_ir_reserve((void**)&f->blocks,&f->block_capacity,f->block_count+1,sizeof(*f->blocks)))return NULL;
 SwanIRBlock *b=&f->blocks[f->block_count++];memset(b,0,sizeof(*b));b->name=name;return b;
}
static inline uint32_t swan_ir_add_inst(SwanIRFunction *f,SwanIRBlock *b,SwanIRInst inst) {
 if(!b||b->terminated||!swan_ir_reserve((void**)&b->instructions,&b->instruction_capacity,b->instruction_count+1,sizeof(*b->instructions)))return 0;
 if(inst.type==SWAN_IR_VOID)inst.result=0;else{if(!swan_ir_value_type(inst.type)||f->next_value==0)return 0;inst.result=f->next_value++;}
 b->instructions[b->instruction_count++]=inst;return inst.result;
}
static inline bool swan_ir_terminate(SwanIRBlock *b,SwanIRTerm term) {
 if(!b||b->terminated)return false;
 b->terminator=term;
 b->terminated=true;
 return true;
}
static inline bool swan_ir_fail(char *error,size_t cap,const char *message,const char *fn,size_t block,size_t inst) {
 if(error&&cap)snprintf(error,cap,"%s in %s, block %zu, instruction %zu",message,fn?fn:"<module>",block,inst);
 return false;
}
static inline SwanIRType swan_ir_value_type_of(const SwanIRFunction *f,uint32_t id) {
 if(!id)return SWAN_IR_VOID;
 if(id<=f->parameter_count)return f->parameter_types[id-1];
 for(size_t b=0;b<f->block_count;b++)for(size_t i=0;i<f->blocks[b].instruction_count;i++)
  if(f->blocks[b].instructions[i].result==id)return f->blocks[b].instructions[i].type;
 return SWAN_IR_VOID;
}
/* A definition dominates a use when every path from entry to the use
 * passes through the definition block. This path test naturally supports
 * forward-numbered blocks and loop backedges without relying on block order. */
static inline bool swan_ir_block_dominates(const SwanIRFunction *f,size_t def_block,size_t use_block) {
 size_t n=f->block_count;
 if(def_block>=n||use_block>=n)return false;
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
  const SwanIRTerm *t=&f->blocks[b].terminator;
  size_t targets[2],count=0;
  if(t->kind==SWAN_IR_BR)targets[count++]=t->target;
  else if(t->kind==SWAN_IR_COND_BR){targets[count++]=t->target;targets[count++]=t->alternate;}
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
  const SwanIRTerm *t=&f->blocks[b].terminator;
  size_t targets[2],count=0;
  if(t->kind==SWAN_IR_BR)targets[count++]=t->target;
  else if(t->kind==SWAN_IR_COND_BR){targets[count++]=t->target;targets[count++]=t->alternate;}
  for(size_t k=0;k<count;k++)if(targets[k]<n&&!seen[targets[k]]){seen[targets[k]]=true;queue[tail++]=targets[k];}
 }
 free(seen);free(queue);
 return !bypass;
}
static inline bool swan_ir_verify_use(const SwanIRFunction *f,uint32_t id,size_t use_block,size_t use_inst,char *error,size_t cap) {
 if(!id)return swan_ir_fail(error,cap,"undefined value",f->name,use_block,use_inst);
 if(id<=f->parameter_count)return true;
 for(size_t b=0;b<f->block_count;b++)for(size_t i=0;i<f->blocks[b].instruction_count;i++){
  const SwanIRInst *def=&f->blocks[b].instructions[i];
  if(def->result!=id)continue;
  if(b==use_block){
   if(i>=use_inst)return swan_ir_fail(error,cap,"value used before its definition",f->name,use_block,use_inst);
   return true;
  }
  if(!swan_ir_block_dominates(f,b,use_block))return swan_ir_fail(error,cap,"definition does not dominate use",f->name,use_block,use_inst);
  return true;
 }
 return swan_ir_fail(error,cap,"use of undefined value",f->name,use_block,use_inst);
}
static inline bool swan_ir_verify(const SwanIRModule *m,char *error,size_t cap) {
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  if(!fn->name||!swan_ir_type_valid(fn->return_type))return swan_ir_fail(error,cap,"invalid function signature",fn->name,0,0);
  if(fn->parameter_count&&!fn->parameter_types)return swan_ir_fail(error,cap,"missing parameter types",fn->name,0,0);
  for(size_t p=0;p<fn->parameter_count;p++)if(!swan_ir_value_type(fn->parameter_types[p]))return swan_ir_fail(error,cap,"invalid parameter type",fn->name,0,p);
  if(!fn->block_count)return swan_ir_fail(error,cap,"function has no entry block",fn->name,0,0);
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRBlock *bb=&fn->blocks[b];
   if(!bb->terminated)return swan_ir_fail(error,cap,"block has no terminator",fn->name,b,bb->instruction_count);
   for(size_t i=0;i<bb->instruction_count;i++){
    const SwanIRInst *in=&bb->instructions[i];
    if((unsigned)in->op>SWAN_IR_CALL||!swan_ir_type_valid(in->type)||(in->type==SWAN_IR_VOID?in->result!=0:in->result==0))return swan_ir_fail(error,cap,"invalid instruction opcode, result type, or ID",fn->name,b,i);
    if(in->result){
     if(in->result<=fn->parameter_count)return swan_ir_fail(error,cap,"instruction redefines a parameter",fn->name,b,i);
     for(size_t db=0;db<=b;db++)for(size_t di=0;di<fn->blocks[db].instruction_count;di++){
      if(db==b&&di>=i)break;
      if(fn->blocks[db].instructions[di].result==in->result)return swan_ir_fail(error,cap,"duplicate SSA value ID",fn->name,b,i);
     }
    }
    if(in->op==SWAN_IR_CONST){if(!swan_ir_value_type(in->type))return swan_ir_fail(error,cap,"constant cannot have void type",fn->name,b,i);}
    else if(in->op==SWAN_IR_PHI){
     if(!in->incoming_count||!in->incoming)return swan_ir_fail(error,cap,"phi has no incoming edges",fn->name,b,i);
     for(size_t k=0;k<in->incoming_count;k++){
      SwanIRIncoming edge=in->incoming[k];
      if(edge.block>=fn->block_count||!swan_ir_verify_use(fn,edge.value,edge.block,fn->blocks[edge.block].instruction_count,error,cap))return false;
      const SwanIRTerm *pred_term=&fn->blocks[edge.block].terminator;
      bool is_pred=(pred_term->kind==SWAN_IR_BR&&pred_term->target==b)||(pred_term->kind==SWAN_IR_COND_BR&&(pred_term->target==b||pred_term->alternate==b));
      if(!is_pred)return swan_ir_fail(error,cap,"phi incoming block is not a predecessor",fn->name,b,i);
      if(swan_ir_value_type_of(fn,edge.value)!=in->type)return swan_ir_fail(error,cap,"phi incoming type mismatch",fn->name,b,i);
      for(size_t j=0;j<k;j++)if(in->incoming[j].block==edge.block)return swan_ir_fail(error,cap,"phi has duplicate predecessor",fn->name,b,i);
     }
    }else if(in->op==SWAN_IR_ALLOCA){if(in->type!=SWAN_IR_PTR)return swan_ir_fail(error,cap,"alloca must produce ptr",fn->name,b,i);}
    else if(in->op==SWAN_IR_CALL){if(!in->symbol)return swan_ir_fail(error,cap,"call has no callee symbol",fn->name,b,i);}
    else if(in->op==SWAN_IR_LOAD||in->op==SWAN_IR_STORE){
     if(!swan_ir_verify_use(fn,in->a,b,i,error,cap)||swan_ir_value_type_of(fn,in->a)!=SWAN_IR_PTR)return swan_ir_fail(error,cap,"memory operation requires a defined pointer",fn->name,b,i);
     if(in->op==SWAN_IR_LOAD&&!swan_ir_value_type(in->type))return swan_ir_fail(error,cap,"load must produce a value",fn->name,b,i);
     if(in->op==SWAN_IR_STORE&&(in->type!=SWAN_IR_VOID||!swan_ir_verify_use(fn,in->b,b,i,error,cap)))return swan_ir_fail(error,cap,"store requires void result and a defined value",fn->name,b,i);
    }else if(in->op==SWAN_IR_SELECT){
     if(!swan_ir_verify_use(fn,in->a,b,i,error,cap)||!swan_ir_verify_use(fn,in->b,b,i,error,cap)||!swan_ir_verify_use(fn,in->c,b,i,error,cap))return false;
     if(swan_ir_value_type_of(fn,in->a)!=SWAN_IR_I1||swan_ir_value_type_of(fn,in->b)!=in->type||swan_ir_value_type_of(fn,in->c)!=in->type)return swan_ir_fail(error,cap,"select operand type mismatch",fn->name,b,i);
    }else if(in->op!=SWAN_IR_PHI&&in->op!=SWAN_IR_ALLOCA&&in->op!=SWAN_IR_CALL&&in->op!=SWAN_IR_LOAD&&in->op!=SWAN_IR_STORE){
     if(!swan_ir_verify_use(fn,in->a,b,i,error,cap)||!swan_ir_verify_use(fn,in->b,b,i,error,cap))return false;
     SwanIRType a=swan_ir_value_type_of(fn,in->a),c=swan_ir_value_type_of(fn,in->b);
     bool cmp=in->op>=SWAN_IR_ICMP_EQ&&in->op<=SWAN_IR_ICMP_SGE;
     if(a!=c||(cmp?in->type!=SWAN_IR_I1:a!=in->type))return swan_ir_fail(error,cap,"binary operand type mismatch",fn->name,b,i);
    }
   }
   const SwanIRTerm *t=&bb->terminator;
   if((unsigned)t->kind>SWAN_IR_UNREACHABLE)return swan_ir_fail(error,cap,"invalid terminator",fn->name,b,bb->instruction_count);
   if((t->kind==SWAN_IR_BR||t->kind==SWAN_IR_COND_BR)&&(t->target>=fn->block_count||(t->kind==SWAN_IR_COND_BR&&t->alternate>=fn->block_count)))return swan_ir_fail(error,cap,"branch target is outside function",fn->name,b,bb->instruction_count);
   if(t->kind==SWAN_IR_COND_BR&&(!swan_ir_verify_use(fn,t->value,b,bb->instruction_count,error,cap)||swan_ir_value_type_of(fn,t->value)!=SWAN_IR_I1))return swan_ir_fail(error,cap,"conditional branch requires i1 condition",fn->name,b,bb->instruction_count);
   if(t->kind==SWAN_IR_RET){
    if(fn->return_type==SWAN_IR_VOID){if(t->value)return swan_ir_fail(error,cap,"void function returns a value",fn->name,b,bb->instruction_count);}
    else {
     if(!t->value)return swan_ir_fail(error,cap,"return value is missing",fn->name,b,bb->instruction_count);
     if(!swan_ir_verify_use(fn,t->value,b,bb->instruction_count,error,cap))return false;
     if(swan_ir_value_type_of(fn,t->value)!=fn->return_type)return swan_ir_fail(error,cap,"return type mismatch",fn->name,b,bb->instruction_count);
    }
   }
  }
 }
 if(error&&cap)error[0]=0;
 return true;
}
#endif
