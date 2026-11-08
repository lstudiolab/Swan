#ifndef SWAN_CODEGEN_AARCH64_H
#define SWAN_CODEGEN_AARCH64_H
#include "swan/codegen_x86_64.h"
#include "swan/ir.h"

static inline int swan_a64_offset(uint32_t id){return (int)(id*8u);}
static inline bool swan_codegen_aarch64_ir(const SwanIRModule *m,Text *o,char *error,size_t cap){
 if(!swan_ir_verify(m,error,cap))return false;
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  if(fn->block_count!=1||fn->blocks[0].terminator.kind!=SWAN_IR_RET||fn->parameter_count>8){
   if(error&&cap)snprintf(error,cap,"AArch64 backend currently requires straight-line functions with at most eight parameters");return false;
  }
 }
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];const SwanIRBlock *bb=&fn->blocks[0];
  int frame=(int)((fn->next_value*8u+15u)&~15u);if(frame<16)frame=16;
  text_add(o,".text\n.p2align 2\n.global %s\n.type %s, %%function\n%s:\n",fn->name,fn->name,fn->name);
  text_add(o,"    stp x29, x30, [sp, #-16]!\n    mov x29, sp\n    sub sp, sp, #%d\n",frame);
  for(size_t p=0;p<fn->parameter_count;p++)text_add(o,"    str x%zu, [sp, #%d]\n",p,swan_a64_offset((uint32_t)p+1));
  for(size_t i=0;i<bb->instruction_count;i++){
   const SwanIRInst *in=&bb->instructions[i];int dst=swan_a64_offset(in->result),a=swan_a64_offset(in->a),b=swan_a64_offset(in->b);
   switch(in->op){
    case SWAN_IR_CONST:text_add(o,"    ldr x9, =%lld\n    str x9, [sp, #%d]\n",(long long)in->immediate,dst);break;
    case SWAN_IR_CALL:
     if(in->incoming_count>8){if(error&&cap)snprintf(error,cap,"AArch64 call has more than eight arguments");return false;}
     for(size_t k=0;k<in->incoming_count;k++)text_add(o,"    ldr x%zu, [sp, #%d]\n",k,swan_a64_offset(in->incoming[k].value));
     text_add(o,"    bl %s\n",in->symbol);
     if(in->type!=SWAN_IR_VOID)text_add(o,"    str x0, [sp, #%d]\n",dst);
     break;
    case SWAN_IR_ADD:case SWAN_IR_SUB:case SWAN_IR_MUL:case SWAN_IR_SDIV:case SWAN_IR_UDIV:
    case SWAN_IR_AND:case SWAN_IR_OR:case SWAN_IR_XOR:case SWAN_IR_SHL:case SWAN_IR_LSHR:case SWAN_IR_ASHR:
     text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n",a,b);
     text_add(o,"    %s x11, x9, x10\n",in->op==SWAN_IR_ADD?"add":in->op==SWAN_IR_SUB?"sub":in->op==SWAN_IR_MUL?"mul":in->op==SWAN_IR_SDIV?"sdiv":in->op==SWAN_IR_UDIV?"udiv":in->op==SWAN_IR_AND?"and":in->op==SWAN_IR_OR?"orr":in->op==SWAN_IR_XOR?"eor":in->op==SWAN_IR_SHL?"lsl":in->op==SWAN_IR_LSHR?"lsr":"asr");
     text_add(o,"    str x11, [sp, #%d]\n",dst);break;
    case SWAN_IR_SREM:case SWAN_IR_UREM:
     text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    %s x11, x9, x10\n    msub x11, x11, x10, x9\n    str x11, [sp, #%d]\n",a,b,in->op==SWAN_IR_SREM?"sdiv":"udiv",dst);break;
    case SWAN_IR_ICMP_EQ:case SWAN_IR_ICMP_NE:case SWAN_IR_ICMP_SLT:case SWAN_IR_ICMP_SLE:case SWAN_IR_ICMP_SGT:case SWAN_IR_ICMP_SGE:
     text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    cmp x9, x10\n    cset x11, %s\n    str x11, [sp, #%d]\n",a,b,in->op==SWAN_IR_ICMP_EQ?"eq":in->op==SWAN_IR_ICMP_NE?"ne":in->op==SWAN_IR_ICMP_SLT?"lt":in->op==SWAN_IR_ICMP_SLE?"le":in->op==SWAN_IR_ICMP_SGT?"gt":"ge",dst);break;
    default:if(error&&cap)snprintf(error,cap,"AArch64 backend encountered unsupported IR instruction");return false;
   }
  }
  if(bb->terminator.value)text_add(o,"    ldr x0, [sp, #%d]\n",swan_a64_offset(bb->terminator.value));else text_add(o,"    mov x0, #0\n");
  text_add(o,"    add sp, sp, #%d\n    ldp x29, x30, [sp], #16\n    ret\n.size %s, .-%s\n",frame,fn->name,fn->name);
 }
 return !o->failed;
}
#endif
