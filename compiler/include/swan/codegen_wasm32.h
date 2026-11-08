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
static inline bool swan_wasm_s64(SwanWasmBuffer *b,int64_t v){bool more=true;while(more){uint8_t x=(uint8_t)(v&127);v>>=7;bool sign=(x&64)!=0;more=!((v==0&&!sign)||(v==-1&&sign));if(more)x|=128;if(!swan_wasm_byte(b,x))return false;}return true;}
static inline bool swan_wasm_name(SwanWasmBuffer *b,const char *s){size_t n=strlen(s);return n<=UINT32_MAX&&swan_wasm_u32(b,(uint32_t)n)&&swan_wasm_bytes(b,s,n);}
static inline uint8_t swan_wasm_type(SwanIRType t){return t==SWAN_IR_I1||t==SWAN_IR_I8||t==SWAN_IR_I16||t==SWAN_IR_I32||t==SWAN_IR_PTR?0x7f:t==SWAN_IR_VOID?0x40:0x7e;}
static inline bool swan_wasm_get(SwanWasmBuffer *b,uint32_t id){return swan_wasm_byte(b,0x20)&&swan_wasm_u32(b,id-1);}
static inline bool swan_wasm_set(SwanWasmBuffer *b,uint32_t id){return swan_wasm_byte(b,0x21)&&swan_wasm_u32(b,id-1);}
static inline bool swan_wasm_emit_inst(SwanWasmBuffer *b,const SwanIRModule *m,const SwanIRInst *in){
 if(in->op==SWAN_IR_CONST)return swan_wasm_byte(b,in->type==SWAN_IR_I1||in->type==SWAN_IR_I32?0x41:0x42)&&swan_wasm_s64(b,in->immediate)&&swan_wasm_set(b,in->result);
 if(in->op==SWAN_IR_CALL){
  size_t index=m->function_count;
  for(size_t i=0;i<m->function_count;i++)if(in->symbol&&!strcmp(m->functions[i].name,in->symbol)){index=i;break;}
  if(index==m->function_count)return false;
  for(size_t i=0;i<in->incoming_count;i++)if(!swan_wasm_get(b,in->incoming[i].value))return false;
  if(!swan_wasm_byte(b,0x10)||!swan_wasm_u32(b,(uint32_t)index))return false;
  return in->type==SWAN_IR_VOID?true:swan_wasm_set(b,in->result);
 }
 if(in->op==SWAN_IR_SELECT)return swan_wasm_get(b,in->b)&&swan_wasm_get(b,in->c)&&swan_wasm_get(b,in->a)&&swan_wasm_byte(b,0x1b)&&swan_wasm_set(b,in->result);
 if(!swan_wasm_get(b,in->a)||!swan_wasm_get(b,in->b))return false;
 uint8_t op=0;bool i32=in->type==SWAN_IR_I1||in->type==SWAN_IR_I8||in->type==SWAN_IR_I16||in->type==SWAN_IR_I32;
 switch(in->op){
  case SWAN_IR_ADD:op=i32?0x6a:0x7c;break;case SWAN_IR_SUB:op=i32?0x6b:0x7d;break;case SWAN_IR_MUL:op=i32?0x6c:0x7e;break;
  case SWAN_IR_SDIV:op=i32?0x6d:0x7f;break;case SWAN_IR_UDIV:op=i32?0x6e:0x80;break;case SWAN_IR_SREM:op=i32?0x6f:0x81;break;case SWAN_IR_UREM:op=i32?0x70:0x82;break;
  case SWAN_IR_AND:op=i32?0x71:0x83;break;case SWAN_IR_OR:op=i32?0x72:0x84;break;case SWAN_IR_XOR:op=i32?0x73:0x85;break;
  case SWAN_IR_SHL:op=i32?0x74:0x86;break;case SWAN_IR_LSHR:op=i32?0x75:0x87;break;case SWAN_IR_ASHR:op=i32?0x76:0x88;break;
  case SWAN_IR_ICMP_EQ:op=i32?0x46:0x51;break;case SWAN_IR_ICMP_NE:op=i32?0x47:0x52;break;case SWAN_IR_ICMP_SLT:op=i32?0x48:0x53;break;
  case SWAN_IR_ICMP_SGT:op=i32?0x4a:0x55;break;case SWAN_IR_ICMP_SLE:op=i32?0x4c:0x57;break;case SWAN_IR_ICMP_SGE:op=i32?0x4e:0x59;break;
  default:return false;
 }
