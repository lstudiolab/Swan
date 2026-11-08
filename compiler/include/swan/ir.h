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
typedef struct { SwanIRFunction *functions; size_t function_count, function_capacity; } SwanIRModule;

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
 free(m->functions);memset(m,0,sizeof(*m));
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
 if(!b||b->terminated)return false;b->terminator=term;b->terminated=true;return true;
}
static inline bool swan_ir_fail(char *error,size_t cap,const char *message,const char *fn,size_t block,size_t inst) {
 if(error&&cap)snprintf(error,cap,"%s in %s, block %zu, instruction %zu",message,fn?fn:"<module>",block,inst);
 return false;
}
static inline bool swan_ir_defined(const SwanIRFunction *f,uint32_t id) {
 if(!id)return false;
 if(id<=f->parameter_count)return true;
 for(size_t b=0;b<f->block_count;b++)for(size_t i=0;i<f->blocks[b].instruction_count;i++)
  if(f->blocks[b].instructions[i].result==id)return true;
 return false;
}
static inline bool swan_ir_verify(const SwanIRModule *m,char *error,size_t cap) {
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  if(!fn->name||!swan_ir_type_valid(fn->return_type))return swan_ir_fail(error,cap,"invalid function signature",fn->name,0,0);
  for(size_t p=0;p<fn->parameter_count;p++)if(!swan_ir_value_type(fn->parameter_types[p]))return swan_ir_fail(error,cap,"invalid parameter type",fn->name,0,p);
  if(!fn->block_count)return swan_ir_fail(error,cap,"function has no entry block",fn->name,0,0);
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRBlock *bb=&fn->blocks[b];
   if(!bb->terminated)return swan_ir_fail(error,cap,"block has no terminator",fn->name,b,bb->instruction_count);
   for(size_t i=0;i<bb->instruction_count;i++){
    const SwanIRInst *in=&bb->instructions[i];
    if(!swan_ir_type_valid(in->type)||(in->type==SWAN_IR_VOID?in->result!=0:in->result==0))return swan_ir_fail(error,cap,"invalid instruction result type or ID",fn->name,b,i);
    if(in->op==SWAN_IR_PHI){
     if(!in->incoming_count||!in->incoming)return swan_ir_fail(error,cap,"phi has no incoming edges",fn->name,b,i);
     for(size_t k=0;k<in->incoming_count;k++){
      SwanIRIncoming edge=in->incoming[k];
      if(edge.block>=fn->block_count||!swan_ir_defined(fn,edge.value))return swan_ir_fail(error,cap,"phi references an invalid value or block",fn->name,b,i);
      for(size_t j=0;j<k;j++)if(in->incoming[j].block==edge.block)return swan_ir_fail(error,cap,"phi has duplicate predecessor",fn->name,b,i);
     }
    }else if(in->op==SWAN_IR_CONST){/* literal */}
    else if(in->op==SWAN_IR_ALLOCA){if(in->type!=SWAN_IR_PTR)return swan_ir_fail(error,cap,"alloca must produce ptr",fn->name,b,i);}
    else if(in->op==SWAN_IR_CALL){if(!in->symbol)return swan_ir_fail(error,cap,"call has no callee symbol",fn->name,b,i);}
    else if(in->op==SWAN_IR_LOAD||in->op==SWAN_IR_STORE){if(!swan_ir_defined(fn,in->a))return swan_ir_fail(error,cap,"memory operation uses undefined pointer",fn->name,b,i);}
    else if(in->op==SWAN_IR_SELECT){if(!swan_ir_defined(fn,in->a)||!swan_ir_defined(fn,in->b)||!swan_ir_defined(fn,in->c))return swan_ir_fail(error,cap,"select uses undefined value",fn->name,b,i);}
    else if(in->op!=SWAN_IR_PHI){
     if(!swan_ir_defined(fn,in->a))return swan_ir_fail(error,cap,"instruction uses undefined operand",fn->name,b,i);
     if(in->op!=SWAN_IR_STORE&&!swan_ir_defined(fn,in->b))return swan_ir_fail(error,cap,"instruction uses undefined operand",fn->name,b,i);
    }
   }
   const SwanIRTerm *t=&bb->terminator;
   if((t->kind==SWAN_IR_BR||t->kind==SWAN_IR_COND_BR)&&(t->target>=fn->block_count||(t->kind==SWAN_IR_COND_BR&&t->alternate>=fn->block_count)))return swan_ir_fail(error,cap,"branch target is outside function",fn->name,b,bb->instruction_count);
   if(t->kind==SWAN_IR_COND_BR&&(!swan_ir_defined(fn,t->value)))return swan_ir_fail(error,cap,"conditional branch uses undefined value",fn->name,b,bb->instruction_count);
   if(t->kind==SWAN_IR_RET&&t->value&&!swan_ir_defined(fn,t->value))return swan_ir_fail(error,cap,"return uses undefined value",fn->name,b,bb->instruction_count);
  }
 }
 if(error&&cap)error[0]=0;return true;
}
#endif
