#ifndef SWAN_CODEGEN_WASM32_H
#define SWAN_CODEGEN_WASM32_H
#include "swan/ir.h"
#include <stdint.h>

typedef struct { unsigned char *data; size_t length,capacity; } SwanWasmBuffer;
static inline bool swan_wasm_reserve(SwanWasmBuffer *b,size_t n){
 if(n>SIZE_MAX-b->length)return false;size_t need=b->length+n;if(need<=b->capacity)return true;
 size_t cap=b->capacity?b->capacity:128;while(cap<need){if(cap>SIZE_MAX/2)return false;cap*=2;}
 void *p=realloc(b->data,cap);if(!p)return false;b->data=p;b->capacity=cap;return true;
}
static inline bool swan_wasm_byte(SwanWasmBuffer *b,uint8_t v){if(!swan_wasm_reserve(b,1))return false;b->data[b->length++]=v;return true;}
static inline bool swan_wasm_bytes(SwanWasmBuffer *b,const void *p,size_t n){if(!swan_wasm_reserve(b,n))return false;memcpy(b->data+b->length,p,n);b->length+=n;return true;}
static inline bool swan_wasm_u32(SwanWasmBuffer *b,uint32_t v){do{uint8_t x=(uint8_t)(v&127u);v>>=7;if(v)x|=128;if(!swan_wasm_byte(b,x))return false;}while(v);return true;}
static inline bool swan_wasm_u64(SwanWasmBuffer *b,uint64_t v){do{uint8_t x=(uint8_t)(v&127u);v>>=7;if(v)x|=128;if(!swan_wasm_byte(b,x))return false;}while(v);return true;}
static inline bool swan_wasm_s64(SwanWasmBuffer *b,int64_t v){bool more=true;while(more){uint8_t x=(uint8_t)(v&127);v>>=7;bool sign=(x&64)!=0;more=!((v==0&&!sign)||(v==-1&&sign));if(more)x|=128;if(!swan_wasm_byte(b,x))return false;}return true;}
static inline bool swan_wasm_name(SwanWasmBuffer *b,const char *s){size_t n=strlen(s);return n<=UINT32_MAX&&swan_wasm_u32(b,(uint32_t)n)&&swan_wasm_bytes(b,s,n);}
static inline uint8_t swan_wasm_type_for(SwanIRType t,bool memory64){return t==SWAN_IR_F32?0x7d:t==SWAN_IR_F64?0x7c:t==SWAN_IR_PTR?(memory64?0x7e:0x7f):t==SWAN_IR_I1||t==SWAN_IR_I8||t==SWAN_IR_I16||t==SWAN_IR_I32?0x7f:t==SWAN_IR_VOID?0x40:0x7e;}
static inline bool swan_wasm_get(SwanWasmBuffer *b,uint32_t id){return id&&swan_wasm_byte(b,0x20)&&swan_wasm_u32(b,id-1);}
static inline bool swan_wasm_set(SwanWasmBuffer *b,uint32_t id){return id&&swan_wasm_byte(b,0x21)&&swan_wasm_u32(b,id-1);}
static inline bool swan_wasm_const(SwanWasmBuffer *b,bool i64,int64_t v){return swan_wasm_byte(b,i64?0x42:0x41)&&swan_wasm_s64(b,v);}
static inline bool swan_wasm_emit_edge(SwanWasmBuffer *b,const SwanIRFunction *fn,size_t from,size_t to){
 const SwanIRBlock *dest=&fn->blocks[to];
 for(size_t i=0;i<dest->instruction_count;i++){const SwanIRInst *phi=&dest->instructions[i];if(phi->op!=SWAN_IR_PHI)continue;
  bool found=false;for(size_t k=0;k<phi->incoming_count;k++)if(phi->incoming[k].block==from){if(!swan_wasm_get(b,phi->incoming[k].value)||!swan_wasm_set(b,phi->result))return false;found=true;break;}
  if(!found)return false;
 }return true;
}
static inline bool swan_wasm_emit_inst(SwanWasmBuffer *b,const SwanIRModule *m,const SwanIRFunction *fn,const SwanIRInst *in,bool memory64,size_t *alloca_index){
 bool i32=in->type==SWAN_IR_I1||in->type==SWAN_IR_I8||in->type==SWAN_IR_I16||in->type==SWAN_IR_I32;
 bool ptr64=memory64;
 if(in->op==SWAN_IR_PHI)return true;
 if(in->op==SWAN_IR_CONST)return swan_wasm_const(b,i32||in->type==SWAN_IR_PTR&&!memory64,in->immediate)&&swan_wasm_set(b,in->result);
 if(in->op==SWAN_IR_ALLOCA){uint64_t addr=16u+(*alloca_index)++*8u;return swan_wasm_const(b,ptr64,(int64_t)addr)&&swan_wasm_set(b,in->result);}
 if(in->op==SWAN_IR_LOAD){return swan_wasm_get(b,in->a)&&swan_wasm_byte(b,in->type==SWAN_IR_I32||in->type==SWAN_IR_I1?0x28:0x29)&&swan_wasm_u32(b,3)&&swan_wasm_u32(b,0)&&swan_wasm_set(b,in->result);}
 if(in->op==SWAN_IR_STORE){SwanIRType vt=swan_ir_value_type_of(fn,in->b);return swan_wasm_get(b,in->a)&&swan_wasm_get(b,in->b)&&swan_wasm_byte(b,vt==SWAN_IR_I32||vt==SWAN_IR_I1?0x36:0x37)&&swan_wasm_u32(b,3)&&swan_wasm_u32(b,0);}
 if(in->op==SWAN_IR_CALL){size_t index=m->function_count;for(size_t i=0;i<m->function_count;i++)if(in->symbol&&!strcmp(m->functions[i].name,in->symbol)){index=i;break;}if(index==m->function_count)return false;
  for(size_t i=0;i<in->incoming_count;i++)if(!swan_wasm_get(b,in->incoming[i].value))return false;
  if(!swan_wasm_byte(b,0x10)||!swan_wasm_u32(b,(uint32_t)index))return false;return in->type==SWAN_IR_VOID||swan_wasm_set(b,in->result);
 }
 if(in->op==SWAN_IR_SELECT)return swan_wasm_get(b,in->b)&&swan_wasm_get(b,in->c)&&swan_wasm_get(b,in->a)&&swan_wasm_byte(b,0x1b)&&swan_wasm_set(b,in->result);
 if(!swan_wasm_get(b,in->a)||!swan_wasm_get(b,in->b))return false;
 SwanIRType operand=swan_ir_value_type_of(fn,in->a);i32=operand==SWAN_IR_I1||operand==SWAN_IR_I8||operand==SWAN_IR_I16||operand==SWAN_IR_I32;
 uint8_t op=0;
 switch(in->op){
  case SWAN_IR_ADD:op=i32?0x6a:0x7c;break;case SWAN_IR_SUB:op=i32?0x6b:0x7d;break;case SWAN_IR_MUL:op=i32?0x6c:0x7e;break;
  case SWAN_IR_SDIV:op=i32?0x6d:0x7f;break;case SWAN_IR_UDIV:op=i32?0x6e:0x80;break;case SWAN_IR_SREM:op=i32?0x6f:0x81;break;case SWAN_IR_UREM:op=i32?0x70:0x82;break;
  case SWAN_IR_AND:op=i32?0x71:0x83;break;case SWAN_IR_OR:op=i32?0x72:0x84;break;case SWAN_IR_XOR:op=i32?0x73:0x85;break;
  case SWAN_IR_SHL:op=i32?0x74:0x86;break;case SWAN_IR_LSHR:op=i32?0x75:0x87;break;case SWAN_IR_ASHR:op=i32?0x76:0x88;break;
  case SWAN_IR_ICMP_EQ:op=i32?0x46:0x51;break;case SWAN_IR_ICMP_NE:op=i32?0x47:0x52;break;case SWAN_IR_ICMP_SLT:op=i32?0x48:0x53;break;
  case SWAN_IR_ICMP_SGT:op=i32?0x4a:0x55;break;case SWAN_IR_ICMP_SLE:op=i32?0x4c:0x57;break;case SWAN_IR_ICMP_SGE:op=i32?0x4e:0x59;break;
  default:return false;
 }return swan_wasm_byte(b,op)&&swan_wasm_set(b,in->result);
}
static inline bool swan_codegen_wasm_target(const SwanIRModule *m,SwanWasmBuffer *out,char *error,size_t cap,bool memory64){
 if(!swan_ir_verify(m,error,cap))return false;
 size_t main_index=m->function_count,alloca_total=0;
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];if(!strcmp(fn->name,"main"))main_index=f;
  if(fn->parameter_count>32){if(error&&cap)snprintf(error,cap,"WebAssembly backend supports at most 32 parameters");return false;}
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
   const SwanIRInst *in=&fn->blocks[b].instructions[i];if(in->op==SWAN_IR_ALLOCA)alloca_total++;
   if(in->type==SWAN_IR_F32||in->type==SWAN_IR_F64){if(error&&cap)snprintf(error,cap,"floating-point IR lowering is not implemented in WebAssembly backend");return false;}
  }
 }
 if(main_index==m->function_count){if(error&&cap)snprintf(error,cap,"WebAssembly module requires a main function export");return false;}
 SwanWasmBuffer type={0},func={0},exports={0},memory={0},code={0},module={0};
 bool ok=swan_wasm_u32(&type,(uint32_t)m->function_count)&&swan_wasm_u32(&func,(uint32_t)m->function_count);
 for(size_t f=0;ok&&f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];ok=swan_wasm_byte(&type,0x60)&&swan_wasm_u32(&type,(uint32_t)fn->parameter_count);
  for(size_t p=0;ok&&p<fn->parameter_count;p++)ok=swan_wasm_byte(&type,swan_wasm_type_for(fn->parameter_types[p],memory64));
  ok=ok&&swan_wasm_u32(&type,fn->return_type==SWAN_IR_VOID?0:1);
  if(ok&&fn->return_type!=SWAN_IR_VOID)ok=swan_wasm_byte(&type,swan_wasm_type_for(fn->return_type,memory64));
  ok=ok&&swan_wasm_u32(&func,(uint32_t)f);
 }
 ok=ok&&swan_wasm_u32(&exports,1)&&swan_wasm_name(&exports,"main")&&swan_wasm_byte(&exports,0)&&swan_wasm_u32(&exports,(uint32_t)main_index);
 if(alloca_total){ok=ok&&swan_wasm_u32(&memory,1)&&swan_wasm_byte(&memory,memory64?4:0)&&swan_wasm_u32(&memory,1);}
 ok=ok&&swan_wasm_u32(&code,(uint32_t)m->function_count);
 for(size_t f=0;ok&&f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];SwanWasmBuffer body={0};uint32_t local_count=fn->next_value-(uint32_t)fn->parameter_count-1;
  ok=swan_wasm_u32(&body,local_count+(fn->block_count>1?1:0));
  for(uint32_t id=(uint32_t)fn->parameter_count+1;ok&&id<fn->next_value;id++){
   SwanIRType t=swan_ir_value_type_of(fn,id);ok=swan_wasm_u32(&body,1)&&swan_wasm_byte(&body,swan_wasm_type_for(t,memory64));
  }
  uint32_t pc=(uint32_t)fn->next_value-1;
  if(ok&&fn->block_count>1)ok=swan_wasm_u32(&body,1)&&swan_wasm_byte(&body,0x7f)&&swan_wasm_const(&body,false,0)&&swan_wasm_byte(&body,0x21)&&swan_wasm_u32(&body,pc);
  size_t alloc_index=0;
  if(ok&&fn->block_count>1){
   ok=swan_wasm_byte(&body,0x03)&&swan_wasm_byte(&body,0x40);
   ok=ok&&swan_wasm_byte(&body,0x02)&&swan_wasm_byte(&body,0x40);
   for(size_t b=fn->block_count;b>0&&ok;b--)ok=swan_wasm_byte(&body,0x02)&&swan_wasm_byte(&body,0x40);
   if(ok){ok=swan_wasm_byte(&body,0x20)&&swan_wasm_u32(&body,pc)&&swan_wasm_byte(&body,0x0e)&&swan_wasm_u32(&body,(uint32_t)fn->block_count);
    for(size_t b=0;ok&&b<fn->block_count;b++)ok=swan_wasm_u32(&body,(uint32_t)b);
    ok=ok&&swan_wasm_u32(&body,(uint32_t)fn->block_count);
   }
   for(size_t b=0;b<fn->block_count&&ok;b++){
    ok=swan_wasm_byte(&body,0x0b);
    const SwanIRBlock *bb=&fn->blocks[b];
    for(size_t i=0;i<bb->instruction_count&&ok;i++)ok=swan_wasm_emit_inst(&body,m,fn,&bb->instructions[i],memory64,&alloc_index);
    const SwanIRTerm *t=&bb->terminator;
    if(!ok)break;
    if(t->kind==SWAN_IR_RET){if(t->value)ok=swan_wasm_get(&body,t->value);ok=ok&&swan_wasm_byte(&body,0x0f);}
    else if(t->kind==SWAN_IR_UNREACHABLE)ok=swan_wasm_byte(&body,0x00);
    else if(t->kind==SWAN_IR_BR){ok=swan_wasm_emit_edge(&body,fn,b,t->target)&&swan_wasm_const(&body,false,(int64_t)t->target)&&swan_wasm_byte(&body,0x21)&&swan_wasm_u32(&body,pc)&&swan_wasm_byte(&body,0x0c)&&swan_wasm_u32(&body,(uint32_t)(fn->block_count-b));}
    else if(t->kind==SWAN_IR_COND_BR){
     ok=swan_wasm_get(&body,t->value)&&swan_wasm_byte(&body,0x04)&&swan_wasm_byte(&body,0x40);
     if(ok)ok=swan_wasm_emit_edge(&body,fn,b,t->target)&&swan_wasm_const(&body,false,(int64_t)t->target)&&swan_wasm_byte(&body,0x21)&&swan_wasm_u32(&body,pc);
     if(ok)ok=swan_wasm_byte(&body,0x05)&&swan_wasm_emit_edge(&body,fn,b,t->alternate)&&swan_wasm_const(&body,false,(int64_t)t->alternate)&&swan_wasm_byte(&body,0x21)&&swan_wasm_u32(&body,pc)&&swan_wasm_byte(&body,0x0b)&&swan_wasm_byte(&body,0x0c)&&swan_wasm_u32(&body,(uint32_t)(fn->block_count-b));
    }else ok=false;
   }
   for(size_t b=0;b<fn->block_count&&ok;b++)ok=swan_wasm_byte(&body,0x0b);
   if(ok)ok=swan_wasm_byte(&body,0x00)&&swan_wasm_byte(&body,0x0b);
  }else if(ok){
   const SwanIRBlock *bb=&fn->blocks[0];
   for(size_t i=0;i<bb->instruction_count&&ok;i++)ok=swan_wasm_emit_inst(&body,m,fn,&bb->instructions[i],memory64,&alloc_index);
   if(ok&&bb->terminator.value)ok=swan_wasm_get(&body,bb->terminator.value);
   if(ok)ok=swan_wasm_byte(&body,bb->terminator.kind==SWAN_IR_UNREACHABLE?0x00:0x0f);
  }
  if(ok)ok=swan_wasm_byte(&body,0x0b)&&swan_wasm_u32(&code,(uint32_t)body.length)&&swan_wasm_bytes(&code,body.data,body.length);
  free(body.data);
 }
 static const uint8_t header[]={0,0x61,0x73,0x6d,1,0,0,0};ok=ok&&swan_wasm_bytes(&module,header,sizeof(header));
 SwanWasmBuffer *sections[]={&type,&func,&memory,&exports,&code};uint8_t ids[]={1,3,5,7,10};
 for(size_t i=0;ok&&i<5;i++)if(sections[i]->length)ok=swan_wasm_byte(&module,ids[i])&&swan_wasm_u32(&module,(uint32_t)sections[i]->length)&&swan_wasm_bytes(&module,sections[i]->data,sections[i]->length);
 free(type.data);free(func.data);free(exports.data);free(memory.data);free(code.data);
 if(!ok){free(module.data);if(error&&cap)snprintf(error,cap,"WebAssembly code generation failed or encountered unsupported IR");return false;}
 *out=module;return true;
}
static inline bool swan_codegen_wasm32(const SwanIRModule *m,SwanWasmBuffer *out,char *error,size_t cap){return swan_codegen_wasm_target(m,out,error,cap,false);}
static inline bool swan_codegen_wasm64(const SwanIRModule *m,SwanWasmBuffer *out,char *error,size_t cap){return swan_codegen_wasm_target(m,out,error,cap,true);}
#endif
