#ifndef BREW_CODEGEN_WASM64_H
#define BREW_CODEGEN_WASM64_H
#include "brew/ir.h"
#include <stdint.h>

typedef struct { unsigned char *data; size_t length,capacity; } BrewWasmBuffer;
static inline bool brew_wasm_reserve(BrewWasmBuffer *b,size_t n){
 if(!b||b->length>b->capacity||n>SIZE_MAX-b->length)return false;
 size_t need=b->length+n;
 if(need<=b->capacity)return true;
 size_t cap=b->capacity?b->capacity:128;
 while(cap<need){
  if(cap>SIZE_MAX/2){cap=need;break;}
  cap*=2;
 }
 void *p=realloc(b->data,cap);
 if(!p)return false;
 b->data=p;b->capacity=cap;
 return true;
}
static inline bool brew_wasm_byte(BrewWasmBuffer *b,uint8_t v){if(!brew_wasm_reserve(b,1))return false;b->data[b->length++]=v;return true;}
static inline bool brew_wasm_bytes(BrewWasmBuffer *b,const void *p,size_t n){if((n&&!p)||!brew_wasm_reserve(b,n))return false;if(n)memcpy(b->data+b->length,p,n);b->length+=n;return true;}
static inline bool brew_wasm_u32(BrewWasmBuffer *b,uint32_t v){do{uint8_t x=(uint8_t)(v&127u);v>>=7;if(v)x|=128;if(!brew_wasm_byte(b,x))return false;}while(v);return true;}
static inline bool brew_wasm_u64(BrewWasmBuffer *b,uint64_t v){do{uint8_t x=(uint8_t)(v&127u);v>>=7;if(v)x|=128;if(!brew_wasm_byte(b,x))return false;}while(v);return true;}
static inline bool brew_wasm_s32(BrewWasmBuffer *b,int32_t value){int64_t v=value;bool more=true;while(more){uint8_t x=(uint8_t)(v&127);v>>=7;bool sign=(x&64)!=0;more=!((v==0&&!sign)||(v==-1&&sign));if(more)x|=128;if(!brew_wasm_byte(b,x))return false;}return true;}
static inline bool brew_wasm_s64(BrewWasmBuffer *b,int64_t v){bool more=true;while(more){uint8_t x=(uint8_t)(v&127);v>>=7;bool sign=(x&64)!=0;more=!((v==0&&!sign)||(v==-1&&sign));if(more)x|=128;if(!brew_wasm_byte(b,x))return false;}return true;}
static inline bool brew_wasm_name(BrewWasmBuffer *b,const char *s){if(!s)return false;size_t n=strlen(s);return n<=UINT32_MAX&&brew_wasm_u32(b,(uint32_t)n)&&brew_wasm_bytes(b,s,n);}
static inline uint8_t brew_wasm_type_for(BrewIRType t,bool memory64){return t==BREW_IR_F32?0x7d:t==BREW_IR_F64?0x7c:t==BREW_IR_PTR?(memory64?0x7e:0x7f):t==BREW_IR_I1||t==BREW_IR_I8||t==BREW_IR_I16||t==BREW_IR_I32?0x7f:t==BREW_IR_VOID?0x40:0x7e;}
static inline bool brew_wasm_get(BrewWasmBuffer *b,uint32_t id){return id&&brew_wasm_byte(b,0x20)&&brew_wasm_u32(b,id-1);}
static inline bool brew_wasm_set(BrewWasmBuffer *b,uint32_t id){return id&&brew_wasm_byte(b,0x21)&&brew_wasm_u32(b,id-1);}
static inline bool brew_wasm_const(BrewWasmBuffer *b,bool i64,int64_t v){if(i64)return brew_wasm_byte(b,0x42)&&brew_wasm_s64(b,v);if(v<INT32_MIN||v>INT32_MAX)return false;return brew_wasm_byte(b,0x41)&&brew_wasm_s32(b,(int32_t)v);}
static inline uint32_t brew_wasm_phi_local_index(const BrewIRFunction *fn,
        size_t target,size_t instruction_index,uint32_t first_local){
 uint32_t ordinal=0;
 for(size_t block=0;block<target;block++)
  for(size_t i=0;i<fn->blocks[block].instruction_count;i++)
   ordinal+=fn->blocks[block].instructions[i].op==BREW_IR_PHI;
 for(size_t i=0;i<instruction_index;i++)
  ordinal+=fn->blocks[target].instructions[i].op==BREW_IR_PHI;
 return first_local+ordinal;
}
static inline bool brew_wasm_emit_edge(BrewWasmBuffer *buffer,
        const BrewIRFunction *function,size_t from,size_t to,
        uint32_t phi_local_base){
 const BrewIRBlock *destination=&function->blocks[to];
 /* Stage every source before writing any phi destination. */
 for(size_t i=0;i<destination->instruction_count;i++){
  const BrewIRInst *phi=&destination->instructions[i];
  if(phi->op!=BREW_IR_PHI)continue;
  bool found=false;
  for(size_t incoming=0;incoming<phi->incoming_count;incoming++){
   if(phi->incoming[incoming].block!=from)continue;
   uint32_t scratch=brew_wasm_phi_local_index(function,to,i,phi_local_base);
   if(!brew_wasm_get(buffer,phi->incoming[incoming].value)||
      !brew_wasm_byte(buffer,0x21)||!brew_wasm_u32(buffer,scratch))return false;
   found=true;
   break;
  }
  if(!found)return false;
 }
 /* Commit the staged values after all source locals have been preserved. */
 for(size_t i=0;i<destination->instruction_count;i++){
  const BrewIRInst *phi=&destination->instructions[i];
  if(phi->op!=BREW_IR_PHI)continue;
  uint32_t scratch=brew_wasm_phi_local_index(function,to,i,phi_local_base);
  if(!brew_wasm_byte(buffer,0x20)||!brew_wasm_u32(buffer,scratch)||
     !brew_wasm_set(buffer,phi->result))return false;
 }
 return true;
}
static inline bool brew_wasm_emit_inst(BrewWasmBuffer *b,const BrewIRModule *m,const BrewIRFunction *fn,const BrewIRInst *in,bool memory64,size_t *alloca_index,uint32_t base_local){
 bool i32=in->type==BREW_IR_I1||in->type==BREW_IR_I8||in->type==BREW_IR_I16||in->type==BREW_IR_I32;
 if(in->op==BREW_IR_PHI)return true;
 if(in->op==BREW_IR_CONST)return brew_wasm_const(b,!i32&&!(in->type==BREW_IR_PTR&&!memory64),in->immediate)&&brew_wasm_set(b,in->result);
 if(in->op==BREW_IR_ALLOCA){uint64_t off=16u+(*alloca_index)++*8u;uint8_t add=memory64?0x7c:0x6a;return brew_wasm_byte(b,0x20)&&brew_wasm_u32(b,base_local)&&brew_wasm_const(b,memory64,(int64_t)off)&&brew_wasm_byte(b,add)&&brew_wasm_set(b,in->result);}
 if(in->op==BREW_IR_LOAD){BrewIRType t=in->type;uint8_t op=t==BREW_IR_I8?0x2c:t==BREW_IR_I16?0x2e:t==BREW_IR_I32||t==BREW_IR_I1?0x28:0x29;uint32_t align=t==BREW_IR_I8?0:t==BREW_IR_I16?1:t==BREW_IR_I32||t==BREW_IR_I1?2:3;return brew_wasm_get(b,in->a)&&brew_wasm_byte(b,op)&&brew_wasm_u32(b,align)&&(memory64?brew_wasm_u64(b,0):brew_wasm_u32(b,0))&&brew_wasm_set(b,in->result);}
 if(in->op==BREW_IR_STORE){BrewIRType t=brew_ir_value_type_of(fn,in->b);uint8_t op=t==BREW_IR_I8?0x3a:t==BREW_IR_I16?0x3b:t==BREW_IR_I32||t==BREW_IR_I1?0x36:0x37;uint32_t align=t==BREW_IR_I8?0:t==BREW_IR_I16?1:t==BREW_IR_I32||t==BREW_IR_I1?2:3;return brew_wasm_get(b,in->a)&&brew_wasm_get(b,in->b)&&brew_wasm_byte(b,op)&&brew_wasm_u32(b,align)&&(memory64?brew_wasm_u64(b,0):brew_wasm_u32(b,0));}
 if(in->op==BREW_IR_CALL){size_t index=m->function_count;for(size_t i=0;i<m->function_count;i++)if(in->symbol&&!strcmp(m->functions[i].name,in->symbol)){index=i;break;}if(index==m->function_count)return false;
  for(size_t i=0;i<in->incoming_count;i++)if(!brew_wasm_get(b,in->incoming[i].value))return false;
  if(!brew_wasm_byte(b,0x10)||!brew_wasm_u32(b,(uint32_t)index))return false;
  return in->type==BREW_IR_VOID||brew_wasm_set(b,in->result);
 }
 if(in->op==BREW_IR_SELECT)return brew_wasm_get(b,in->b)&&brew_wasm_get(b,in->c)&&brew_wasm_get(b,in->a)&&brew_wasm_byte(b,0x1b)&&brew_wasm_set(b,in->result);
 if(in->op==BREW_IR_SDIV||in->op==BREW_IR_SREM){
  BrewIRType operand_type=brew_ir_value_type_of(fn,in->a);
  bool integer32=operand_type==BREW_IR_I1||operand_type==BREW_IR_I8||
                 operand_type==BREW_IR_I16||operand_type==BREW_IR_I32;
  int64_t minimum=integer32?INT32_MIN:INT64_MIN;
  if(!brew_wasm_get(b,in->a)||
     !brew_wasm_const(b,!integer32,minimum)||!brew_wasm_byte(b,integer32?0x46:0x51)||
     !brew_wasm_get(b,in->b)||!brew_wasm_const(b,!integer32,-1)||
     !brew_wasm_byte(b,integer32?0x46:0x51)||
     !brew_wasm_byte(b,0x71)||
     !brew_wasm_byte(b,0x04)||!brew_wasm_byte(b,0x40)||
     !brew_wasm_byte(b,0x00)||!brew_wasm_byte(b,0x0b))return false;
 }
 if(!brew_wasm_get(b,in->a)||!brew_wasm_get(b,in->b))return false;
 BrewIRType operand=brew_ir_value_type_of(fn,in->a);i32=operand==BREW_IR_I1||operand==BREW_IR_I8||operand==BREW_IR_I16||operand==BREW_IR_I32;
 uint8_t op=0;
 switch(in->op){
  case BREW_IR_ADD:op=i32?0x6a:0x7c;break;case BREW_IR_SUB:op=i32?0x6b:0x7d;break;case BREW_IR_MUL:op=i32?0x6c:0x7e;break;
  case BREW_IR_SDIV:op=i32?0x6d:0x7f;break;case BREW_IR_UDIV:op=i32?0x6e:0x80;break;case BREW_IR_SREM:op=i32?0x6f:0x81;break;case BREW_IR_UREM:op=i32?0x70:0x82;break;
  case BREW_IR_AND:op=i32?0x71:0x83;break;case BREW_IR_OR:op=i32?0x72:0x84;break;case BREW_IR_XOR:op=i32?0x73:0x85;break;
  case BREW_IR_SHL:op=i32?0x74:0x86;break;case BREW_IR_LSHR:op=i32?0x75:0x87;break;case BREW_IR_ASHR:op=i32?0x76:0x88;break;
  case BREW_IR_ICMP_EQ:op=i32?0x46:0x51;break;case BREW_IR_ICMP_NE:op=i32?0x47:0x52;break;case BREW_IR_ICMP_SLT:op=i32?0x48:0x53;break;
  case BREW_IR_ICMP_SGT:op=i32?0x4a:0x55;break;case BREW_IR_ICMP_SLE:op=i32?0x4c:0x57;break;case BREW_IR_ICMP_SGE:op=i32?0x4e:0x59;break;
  default:return false;
 }return brew_wasm_byte(b,op)&&brew_wasm_set(b,in->result);
}
static inline bool brew_wasm_emit_inst_checked(BrewWasmBuffer *buffer,const BrewIRModule *module,const BrewIRFunction *function,const BrewIRInst *instruction,bool memory64,size_t *alloca_index,uint32_t base_local,char *error,size_t cap,size_t block_index,size_t instruction_index){
 if(brew_wasm_emit_inst(buffer,module,function,instruction,memory64,alloca_index,base_local))return true;
 if(error&&cap)snprintf(error,cap,"WebAssembly lowering failed in function '%s', block %zu, instruction %zu (opcode %d, type %d, immediate %lld)",function->name,block_index,instruction_index,(int)instruction->op,(int)instruction->type,(long long)instruction->immediate);
 return false;
}
static inline bool brew_codegen_wasm_target(const BrewIRModule *m,BrewWasmBuffer *out,char *error,size_t cap,bool memory64){
 if(error&&cap)error[0]=0;
 if(!m||!out){if(error&&cap)snprintf(error,cap,"WebAssembly code generation requires a module and output buffer");return false;}
 memset(out,0,sizeof(*out));
 if(!brew_ir_verify(m,error,cap))return false;
 size_t main_index=m->function_count,alloca_total=0;
 for(size_t f=0;f<m->function_count;f++){
  const BrewIRFunction *fn=&m->functions[f];
  if(fn->external){if(error&&cap)snprintf(error,cap,"WebAssembly backend does not yet support external function imports");return false;}
  if(!strcmp(fn->name,"main"))main_index=f;
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
   const BrewIRInst *in=&fn->blocks[b].instructions[i];if(in->op==BREW_IR_ALLOCA)alloca_total++;
   if(in->type==BREW_IR_F32||in->type==BREW_IR_F64){if(error&&cap)snprintf(error,cap,"floating-point IR lowering is not implemented in WebAssembly backend");return false;}
  }
 }
 if(main_index==m->function_count&&!m->allow_no_main){if(error&&cap)snprintf(error,cap,"WebAssembly module requires a main function export");return false;}
 BrewWasmBuffer type={0},func={0},exports={0},memory={0},globals={0},code={0},module={0};
 bool ok=brew_wasm_u32(&type,(uint32_t)m->function_count)&&brew_wasm_u32(&func,(uint32_t)m->function_count);
 for(size_t f=0;ok&&f<m->function_count;f++){
  const BrewIRFunction *fn=&m->functions[f];ok=brew_wasm_byte(&type,0x60)&&brew_wasm_u32(&type,(uint32_t)fn->parameter_count);
  for(size_t p=0;ok&&p<fn->parameter_count;p++)ok=brew_wasm_byte(&type,brew_wasm_type_for(fn->parameter_types[p],memory64));
  ok=ok&&brew_wasm_u32(&type,fn->return_type==BREW_IR_VOID?0:1);
  if(ok&&fn->return_type!=BREW_IR_VOID)ok=brew_wasm_byte(&type,brew_wasm_type_for(fn->return_type,memory64));
  ok=ok&&brew_wasm_u32(&func,(uint32_t)f);
 }
 size_t export_count=0;
 for(size_t f=0;f<m->function_count;f++)
  if(f==main_index||m->functions[f].exported)export_count++;
 ok=ok&&brew_wasm_u32(&exports,(uint32_t)export_count);
 for(size_t f=0;ok&&f<m->function_count;f++){
  const BrewIRFunction *fn=&m->functions[f];
  if(f!=main_index&&!fn->exported)continue;
  const char *export_name=f==main_index?"main":fn->name;
  ok=brew_wasm_name(&exports,export_name)&&brew_wasm_byte(&exports,0)&&
     brew_wasm_u32(&exports,(uint32_t)f);
 }
 if(alloca_total){
  uint64_t max_frame=0;
  for(size_t f=0;f<m->function_count;f++){size_t n=0;for(size_t b=0;b<m->functions[f].block_count;b++)for(size_t i=0;i<m->functions[f].blocks[b].instruction_count;i++)n+=m->functions[f].blocks[b].instructions[i].op==BREW_IR_ALLOCA;if(n*8u+16u>max_frame)max_frame=n*8u+16u;}
  uint64_t pages=(16u+max_frame*256u+65535u)/65536u;
  ok=ok&&brew_wasm_u32(&memory,1)&&brew_wasm_byte(&memory,memory64?4:0)&&(memory64?brew_wasm_u64(&memory,pages):brew_wasm_u32(&memory,(uint32_t)pages));
  ok=ok&&brew_wasm_u32(&globals,1)&&brew_wasm_byte(&globals,brew_wasm_type_for(BREW_IR_PTR,memory64))&&brew_wasm_byte(&globals,1)&&brew_wasm_const(&globals,memory64,16)&&brew_wasm_byte(&globals,0x0b);
 }
 ok=ok&&brew_wasm_u32(&code,(uint32_t)m->function_count);
 for(size_t f=0;ok&&f<m->function_count;f++){
  const BrewIRFunction *fn=&m->functions[f];BrewWasmBuffer body={0};
  uint32_t value_locals=fn->next_value-(uint32_t)fn->parameter_count-1;
  size_t fn_allocas=0,phi_count=0;
  for(size_t b=0;b<fn->block_count;b++)
   for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
    fn_allocas+=fn->blocks[b].instructions[i].op==BREW_IR_ALLOCA;
    phi_count+=fn->blocks[b].instructions[i].op==BREW_IR_PHI;
   }
  uint32_t base_local=(uint32_t)fn->next_value-1,pc=base_local+(fn_allocas?1u:0u);
  uint32_t phi_local_base=pc+(fn->block_count>1?1u:0u);
  ok=brew_wasm_u32(&body,value_locals+(fn_allocas?1u:0u)+(fn->block_count>1?1u:0u)+(uint32_t)phi_count);
  for(uint32_t id=(uint32_t)fn->parameter_count+1;ok&&id<fn->next_value;id++){
   BrewIRType t=brew_ir_value_type_of(fn,id);if(t==BREW_IR_VOID)t=BREW_IR_I64;ok=brew_wasm_u32(&body,1)&&brew_wasm_byte(&body,brew_wasm_type_for(t,memory64));
  }
  if(ok&&fn_allocas)ok=brew_wasm_u32(&body,1)&&brew_wasm_byte(&body,brew_wasm_type_for(BREW_IR_PTR,memory64));
  if(ok&&fn->block_count>1)
   ok=brew_wasm_u32(&body,1)&&brew_wasm_byte(&body,0x7f);
  for(size_t b=0;ok&&b<fn->block_count;b++)
   for(size_t i=0;ok&&i<fn->blocks[b].instruction_count;i++){
    const BrewIRInst *phi=&fn->blocks[b].instructions[i];
    if(phi->op==BREW_IR_PHI)
     ok=brew_wasm_u32(&body,1)&&
        brew_wasm_byte(&body,brew_wasm_type_for(phi->type,memory64));
   }
  /* WebAssembly requires every local declaration before the first opcode. */
  if(ok&&fn->block_count>1)
   ok=brew_wasm_const(&body,false,0)&&brew_wasm_byte(&body,0x21)&&brew_wasm_u32(&body,pc);
  size_t alloc_index=0;
  if(ok&&fn_allocas)ok=brew_wasm_byte(&body,0x23)&&brew_wasm_u32(&body,0)&&brew_wasm_byte(&body,0x22)&&brew_wasm_u32(&body,base_local)&&brew_wasm_const(&body,memory64,(int64_t)(fn_allocas*8u+16u))&&brew_wasm_byte(&body,memory64?0x7c:0x6a)&&brew_wasm_byte(&body,0x24)&&brew_wasm_u32(&body,0);
  if(ok&&fn->block_count>1){
   ok=brew_wasm_byte(&body,0x03)&&brew_wasm_byte(&body,0x40);
   ok=ok&&brew_wasm_byte(&body,0x02)&&brew_wasm_byte(&body,0x40);
   for(size_t b=fn->block_count;b>0&&ok;b--)ok=brew_wasm_byte(&body,0x02)&&brew_wasm_byte(&body,0x40);
   if(ok){ok=brew_wasm_byte(&body,0x20)&&brew_wasm_u32(&body,pc)&&brew_wasm_byte(&body,0x0e)&&brew_wasm_u32(&body,(uint32_t)fn->block_count);
    for(size_t b=0;ok&&b<fn->block_count;b++)ok=brew_wasm_u32(&body,(uint32_t)b);
    ok=ok&&brew_wasm_u32(&body,(uint32_t)fn->block_count);
   }
   for(size_t b=0;b<fn->block_count&&ok;b++){
    ok=brew_wasm_byte(&body,0x0b);
    const BrewIRBlock *bb=&fn->blocks[b];
    for(size_t i=0;i<bb->instruction_count&&ok;i++)ok=brew_wasm_emit_inst_checked(&body,m,fn,&bb->instructions[i],memory64,&alloc_index,base_local,error,cap,b,i);
    const BrewIRTerm *t=&bb->terminator;
    if(!ok)break;
    if(t->kind==BREW_IR_RET){if(t->value)ok=brew_wasm_get(&body,t->value);if(ok&&fn_allocas)ok=brew_wasm_byte(&body,0x20)&&brew_wasm_u32(&body,base_local)&&brew_wasm_byte(&body,0x24)&&brew_wasm_u32(&body,0);ok=ok&&brew_wasm_byte(&body,0x0f);}
    else if(t->kind==BREW_IR_UNREACHABLE)ok=brew_wasm_byte(&body,0x00);
    else if(t->kind==BREW_IR_BR){ok=brew_wasm_emit_edge(&body,fn,b,t->target,phi_local_base)&&brew_wasm_const(&body,false,(int64_t)t->target)&&brew_wasm_byte(&body,0x21)&&brew_wasm_u32(&body,pc)&&brew_wasm_byte(&body,0x0c)&&brew_wasm_u32(&body,(uint32_t)(fn->block_count-b));}
    else if(t->kind==BREW_IR_COND_BR){
     ok=brew_wasm_get(&body,t->value)&&brew_wasm_byte(&body,0x04)&&brew_wasm_byte(&body,0x40);
     if(ok)ok=brew_wasm_emit_edge(&body,fn,b,t->target,phi_local_base)&&brew_wasm_const(&body,false,(int64_t)t->target)&&brew_wasm_byte(&body,0x21)&&brew_wasm_u32(&body,pc);
     if(ok)ok=brew_wasm_byte(&body,0x05)&&brew_wasm_emit_edge(&body,fn,b,t->alternate,phi_local_base)&&brew_wasm_const(&body,false,(int64_t)t->alternate)&&brew_wasm_byte(&body,0x21)&&brew_wasm_u32(&body,pc)&&brew_wasm_byte(&body,0x0b)&&brew_wasm_byte(&body,0x0c)&&brew_wasm_u32(&body,(uint32_t)(fn->block_count-b));
    }else ok=false;
   }
   if(ok)ok=brew_wasm_byte(&body,0x0b)&&brew_wasm_byte(&body,0x00)&&brew_wasm_byte(&body,0x0b)&&brew_wasm_byte(&body,0x00);
  }else if(ok){
   const BrewIRBlock *bb=&fn->blocks[0];
   if(bb->terminator.kind!=BREW_IR_RET&&bb->terminator.kind!=BREW_IR_UNREACHABLE){
    if(error&&cap)snprintf(error,cap,"WebAssembly backend does not yet support single-block branch terminators");
    ok=false;
   }
   for(size_t i=0;i<bb->instruction_count&&ok;i++)ok=brew_wasm_emit_inst_checked(&body,m,fn,&bb->instructions[i],memory64,&alloc_index,base_local,error,cap,0,i);
   if(ok&&bb->terminator.value)ok=brew_wasm_get(&body,bb->terminator.value);
   if(ok&&fn_allocas)ok=brew_wasm_byte(&body,0x20)&&brew_wasm_u32(&body,base_local)&&brew_wasm_byte(&body,0x24)&&brew_wasm_u32(&body,0);
   if(ok)ok=brew_wasm_byte(&body,bb->terminator.kind==BREW_IR_UNREACHABLE?0x00:0x0f);
  }
  if(ok)ok=brew_wasm_byte(&body,0x0b)&&brew_wasm_u32(&code,(uint32_t)body.length)&&brew_wasm_bytes(&code,body.data,body.length);
  free(body.data);
 }
 static const uint8_t header[]={0,0x61,0x73,0x6d,1,0,0,0};ok=ok&&brew_wasm_bytes(&module,header,sizeof(header));
 BrewWasmBuffer *sections[]={&type,&func,&memory,&globals,&exports,&code};uint8_t ids[]={1,3,5,6,7,10};
 for(size_t i=0;ok&&i<6;i++)if(sections[i]->length)ok=brew_wasm_byte(&module,ids[i])&&brew_wasm_u32(&module,(uint32_t)sections[i]->length)&&brew_wasm_bytes(&module,sections[i]->data,sections[i]->length);
 free(type.data);free(func.data);free(exports.data);free(memory.data);free(globals.data);free(code.data);
 if(!ok){free(module.data);if(error&&cap&&!error[0])snprintf(error,cap,"WebAssembly code generation failed while emitting module sections or control flow");return false;}
 *out=module;return true;
}
static inline bool brew_codegen_wasm64(const BrewIRModule *m,BrewWasmBuffer *out,char *error,size_t cap){return brew_codegen_wasm_target(m,out,error,cap,true);}
#endif
