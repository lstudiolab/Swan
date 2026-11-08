#ifndef SWAN_CODEGEN_ARMV7_H
#define SWAN_CODEGEN_ARMV7_H
#include "swan/codegen_x86_64.h"
#include "swan/ir.h"

static inline int swan_armv7_offset(uint32_t id){return (int)(id*4u);}
static inline bool swan_codegen_armv7_ir(const SwanIRModule *m,Text *o,char *error,size_t cap){
 if(!swan_ir_verify(m,error,cap))return false;
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  if(fn->block_count!=1||fn->blocks[0].terminator.kind!=SWAN_IR_RET||fn->parameter_count>4||
     (fn->return_type!=SWAN_IR_VOID&&fn->return_type!=SWAN_IR_I32)){
   if(error&&cap)snprintf(error,cap,"ARMv7 backend currently requires straight-line i32 functions with at most four parameters");
   return false;
  }
  for(size_t p=0;p<fn->parameter_count;p++)if(fn->parameter_types[p]!=SWAN_IR_I32){
   if(error&&cap)snprintf(error,cap,"ARMv7 backend supports i32 parameters only");return false;
  }
  for(size_t i=0;i<fn->blocks[0].instruction_count;i++){
   const SwanIRInst *in=&fn->blocks[0].instructions[i];
   if(in->type!=SWAN_IR_I32&&in->type!=SWAN_IR_I1){
    if(error&&cap)snprintf(error,cap,"ARMv7 backend supports i32 and i1 values only");return false;
   }
   if(in->op==SWAN_IR_SDIV||in->op==SWAN_IR_UDIV||in->op==SWAN_IR_SREM||in->op==SWAN_IR_UREM){
    if(error&&cap)snprintf(error,cap,"ARMv7 integer division requires a target runtime helper and is not yet supported");return false;
   }
  }
 }
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];const SwanIRBlock *bb=&fn->blocks[0];
  int frame=(int)((fn->next_value*4u+7u)&~7u);if(frame<8)frame=8;
  text_add(o,".syntax unified\n.text\n.align 2\n.global %s\n.type %s, %%function\n%s:\n    push {r4, lr}\n    sub sp, sp, #%d\n",fn->name,fn->name,fn->name,frame);
  for(size_t p=0;p<fn->parameter_count;p++)text_add(o,"    str r%zu, [sp, #%d]\n",p,swan_armv7_offset((uint32_t)p+1));
  for(size_t i=0;i<bb->instruction_count;i++){
   const SwanIRInst *in=&bb->instructions[i];int dst=swan_armv7_offset(in->result),a=swan_armv7_offset(in->a),b=swan_armv7_offset(in->b);
   switch(in->op){
    case SWAN_IR_CONST:text_add(o,"    ldr r0, =%d\n    str r0, [sp, #%d]\n",(int32_t)in->immediate,dst);break;
    case SWAN_IR_CALL:
     if(in->incoming_count>4){if(error&&cap)snprintf(error,cap,"ARMv7 calls support at most four arguments");return false;}
     for(size_t k=0;k<in->incoming_count;k++)text_add(o,"    ldr r%zu, [sp, #%d]\n",k,swan_armv7_offset(in->incoming[k].value));
     text_add(o,"    bl %s\n",in->symbol);if(in->type!=SWAN_IR_VOID)text_add(o,"    str r0, [sp, #%d]\n",dst);break;
    case SWAN_IR_ADD:case SWAN_IR_SUB:case SWAN_IR_MUL:case SWAN_IR_AND:case SWAN_IR_OR:case SWAN_IR_XOR:case SWAN_IR_SHL:case SWAN_IR_LSHR:case SWAN_IR_ASHR:
     text_add(o,"    ldr r2, [sp, #%d]\n    ldr r3, [sp, #%d]\n    %s r1, r2, r3\n    str r1, [sp, #%d]\n",a,b,
      in->op==SWAN_IR_ADD?"add":in->op==SWAN_IR_SUB?"sub":in->op==SWAN_IR_MUL?"mul":in->op==SWAN_IR_AND?"and":in->op==SWAN_IR_OR?"orr":in->op==SWAN_IR_XOR?"eor":in->op==SWAN_IR_SHL?"lsl":in->op==SWAN_IR_LSHR?"lsr":"asr",dst);break;
    case SWAN_IR_ICMP_EQ:case SWAN_IR_ICMP_NE:case SWAN_IR_ICMP_SLT:case SWAN_IR_ICMP_SLE:case SWAN_IR_ICMP_SGT:case SWAN_IR_ICMP_SGE:
     text_add(o,"    ldr r2, [sp, #%d]\n    ldr r3, [sp, #%d]\n    cmp r2, r3\n    mov r1, #0\n    mov%s r1, #1\n    str r1, [sp, #%d]\n",a,b,
      in->op==SWAN_IR_ICMP_EQ?"eq":in->op==SWAN_IR_ICMP_NE?"ne":in->op==SWAN_IR_ICMP_SLT?"lt":in->op==SWAN_IR_ICMP_SLE?"le":in->op==SWAN_IR_ICMP_SGT?"gt":"ge",dst);break;
    default:if(error&&cap)snprintf(error,cap,"ARMv7 backend encountered unsupported IR instruction");return false;
   }
  }
  if(bb->terminator.value)text_add(o,"    ldr r0, [sp, #%d]\n",swan_armv7_offset(bb->terminator.value));else text_add(o,"    mov r0, #0\n");
  text_add(o,"    add sp, sp, #%d\n    pop {r4, pc}\n.size %s, .-%s\n",frame,fn->name,fn->name);
 }
 return !o->failed;
}
#endif
