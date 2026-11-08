#ifndef SWAN_CODEGEN_AARCH64_H
#define SWAN_CODEGEN_AARCH64_H
#include "swan/codegen_x86_64.h"
#include "swan/ir.h"

static inline int swan_a64_value_offset(uint32_t id){return (int)(id*8u);}
static inline bool swan_a64_edge_has_phi(const SwanIRFunction *fn,size_t target){
 const SwanIRBlock *b=&fn->blocks[target];
 for(size_t i=0;i<b->instruction_count;i++)if(b->instructions[i].op==SWAN_IR_PHI)return true;
 return false;
}
static inline bool swan_codegen_aarch64_ir(const SwanIRModule *m,Text *o,char *error,size_t cap){
 if(!swan_ir_verify(m,error,cap))return false;
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  if(fn->parameter_count>8){if(error&&cap)snprintf(error,cap,"AArch64 ABI supports at most eight register parameters");return false;}
  if(fn->return_type!=SWAN_IR_VOID&&fn->return_type!=SWAN_IR_I1&&fn->return_type!=SWAN_IR_I64){if(error&&cap)snprintf(error,cap,"AArch64 backend currently supports i1 and i64 return values");return false;}
  for(size_t p=0;p<fn->parameter_count;p++)if(fn->parameter_types[p]!=SWAN_IR_I1&&fn->parameter_types[p]!=SWAN_IR_I64){if(error&&cap)snprintf(error,cap,"AArch64 backend currently supports i1 and i64 parameters");return false;}
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
   const SwanIRInst *in=&fn->blocks[b].instructions[i];
   if(in->type!=SWAN_IR_VOID&&in->type!=SWAN_IR_I1&&in->type!=SWAN_IR_I64&&in->type!=SWAN_IR_PTR){if(error&&cap)snprintf(error,cap,"AArch64 backend currently supports i1/i64 scalar values and pointers");return false;}
   if(in->op==SWAN_IR_CALL&&in->incoming_count>8){if(error&&cap)snprintf(error,cap,"AArch64 calls support at most eight register arguments");return false;}
  }
 }
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  size_t allocas=0;for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++)allocas+=fn->blocks[b].instructions[i].op==SWAN_IR_ALLOCA;
  size_t value_bytes=((size_t)fn->next_value+1)*8u,frame=(value_bytes+allocas*8u+15u)&~(size_t)15u;
  if(frame<16)frame=16;
  text_add(o,".text\n.p2align 2\n.global %s\n.type %s, %%function\n%s:\n    stp x29, x30, [sp, #-16]!\n    mov x29, sp\n    sub sp, sp, #%zu\n",fn->name,fn->name,fn->name,frame);
  for(size_t p=0;p<fn->parameter_count;p++)text_add(o,"    str x%zu, [sp, #%d]\n",p,swan_a64_value_offset((uint32_t)p+1));
  size_t ai=0;
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRBlock *bb=&fn->blocks[b];
   text_add(o,".Lsw_f%zu_b%zu:\n",f,b);
   for(size_t i=0;i<bb->instruction_count;i++){
    const SwanIRInst *in=&bb->instructions[i];int dst=swan_a64_value_offset(in->result),a=swan_a64_value_offset(in->a),z=swan_a64_value_offset(in->b);
    switch(in->op){
     case SWAN_IR_PHI:break;
     case SWAN_IR_CONST:text_add(o,"    ldr x9, =%lld\n    str x9, [sp, #%d]\n",(long long)in->immediate,dst);break;
     case SWAN_IR_ALLOCA:{size_t off=value_bytes+ai++*8u;text_add(o,"    add x9, sp, #%zu\n    str x9, [sp, #%d]\n",off,dst);break;}
     case SWAN_IR_LOAD:text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [x9]\n    str x10, [sp, #%d]\n",a,dst);break;
     case SWAN_IR_STORE:text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    str x10, [x9]\n",a,z);break;
     case SWAN_IR_CALL:
      for(size_t k=0;k<in->incoming_count;k++)text_add(o,"    ldr x%zu, [sp, #%d]\n",k,swan_a64_value_offset(in->incoming[k].value));
      text_add(o,"    bl %s\n",in->symbol);if(in->type!=SWAN_IR_VOID)text_add(o,"    str x0, [sp, #%d]\n",dst);break;
     case SWAN_IR_SELECT:text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    ldr x11, [sp, #%d]\n    cmp x9, #0\n    csel x12, x10, x11, ne\n    str x12, [sp, #%d]\n",a,z,swan_a64_value_offset(in->c),dst);break;
     case SWAN_IR_ADD:case SWAN_IR_SUB:case SWAN_IR_MUL:case SWAN_IR_SDIV:case SWAN_IR_UDIV:case SWAN_IR_AND:case SWAN_IR_OR:case SWAN_IR_XOR:case SWAN_IR_SHL:case SWAN_IR_LSHR:case SWAN_IR_ASHR:
      text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    %s x11, x9, x10\n    str x11, [sp, #%d]\n",a,z,in->op==SWAN_IR_ADD?"add":in->op==SWAN_IR_SUB?"sub":in->op==SWAN_IR_MUL?"mul":in->op==SWAN_IR_SDIV?"sdiv":in->op==SWAN_IR_UDIV?"udiv":in->op==SWAN_IR_AND?"and":in->op==SWAN_IR_OR?"orr":in->op==SWAN_IR_XOR?"eor":in->op==SWAN_IR_SHL?"lsl":in->op==SWAN_IR_LSHR?"lsr":"asr",dst);break;
     case SWAN_IR_SREM:case SWAN_IR_UREM:text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    %s x11, x9, x10\n    msub x11, x11, x10, x9\n    str x11, [sp, #%d]\n",a,z,in->op==SWAN_IR_SREM?"sdiv":"udiv",dst);break;
     case SWAN_IR_ICMP_EQ:case SWAN_IR_ICMP_NE:case SWAN_IR_ICMP_SLT:case SWAN_IR_ICMP_SLE:case SWAN_IR_ICMP_SGT:case SWAN_IR_ICMP_SGE:
      text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    cmp x9, x10\n    cset x11, %s\n    str x11, [sp, #%d]\n",a,z,in->op==SWAN_IR_ICMP_EQ?"eq":in->op==SWAN_IR_ICMP_NE?"ne":in->op==SWAN_IR_ICMP_SLT?"lt":in->op==SWAN_IR_ICMP_SLE?"le":in->op==SWAN_IR_ICMP_SGT?"gt":"ge",dst);break;
     default:if(error&&cap)snprintf(error,cap,"AArch64 backend encountered unsupported IR opcode %u",(unsigned)in->op);return false;
    }
   }
   const SwanIRTerm *t=&bb->terminator;
   if(t->kind==SWAN_IR_RET){if(t->value)text_add(o,"    ldr x0, [sp, #%d]\n",swan_a64_value_offset(t->value));else text_add(o,"    mov x0, #0\n");text_add(o,"    add sp, sp, #%zu\n    ldp x29, x30, [sp], #16\n    ret\n",frame);}
   else if(t->kind==SWAN_IR_BR)text_add(o,"    b .Lsw_e%zu_%zu_%u\n",f,b,t->target);
   else if(t->kind==SWAN_IR_COND_BR)text_add(o,"    ldr x9, [sp, #%d]\n    cbnz x9, .Lsw_e%zu_%zu_%u\n    b .Lsw_e%zu_%zu_%u\n",swan_a64_value_offset(t->value),f,b,t->target,f,b,t->alternate);
   else text_add(o,"    brk #0\n");
  }
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRTerm *t=&fn->blocks[b].terminator;size_t targets[2],n=0;
   if(t->kind==SWAN_IR_BR)targets[n++]=t->target;else if(t->kind==SWAN_IR_COND_BR){targets[n++]=t->target;if(t->alternate!=t->target)targets[n++]=t->alternate;}
   for(size_t k=0;k<n;k++){size_t target=targets[k];text_add(o,".Lsw_e%zu_%zu_%zu:\n",f,b,target);
    const SwanIRBlock *dest=&fn->blocks[target];
    for(size_t i=0;i<dest->instruction_count;i++){const SwanIRInst *phi=&dest->instructions[i];if(phi->op!=SWAN_IR_PHI)continue;
     for(size_t q=0;q<phi->incoming_count;q++)if(phi->incoming[q].block==b){text_add(o,"    ldr x9, [sp, #%d]\n    str x9, [sp, #%d]\n",swan_a64_value_offset(phi->incoming[q].value),swan_a64_value_offset(phi->result));break;}
    }
    text_add(o,"    b .Lsw_f%zu_b%zu\n",f,target);
   }
  }
  text_add(o,".size %s, .-%s\n",fn->name,fn->name);
 }
 return !o->failed;
}
#endif
