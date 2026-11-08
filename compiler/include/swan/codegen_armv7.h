#ifndef SWAN_CODEGEN_ARMV7_H
#define SWAN_CODEGEN_ARMV7_H
#include "swan/codegen_x86_64.h"
#include "swan/ir.h"
static inline int swan_armv7_offset(uint32_t id){return (int)(id*4u);}
static inline bool swan_codegen_armv7_ir(const SwanIRModule *m,Text *o,char *error,size_t cap){
 if(!swan_ir_verify(m,error,cap))return false;
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  if(fn->parameter_count>4||fn->return_type==SWAN_IR_I64||fn->return_type==SWAN_IR_F32||fn->return_type==SWAN_IR_F64){if(error&&cap)snprintf(error,cap,"ARMv7 backend supports at most four i32 parameters and i32/void returns");return false;}
  for(size_t p=0;p<fn->parameter_count;p++)if(fn->parameter_types[p]!=SWAN_IR_I32){if(error&&cap)snprintf(error,cap,"ARMv7 backend supports i32 parameters only");return false;}
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
   const SwanIRInst *in=&fn->blocks[b].instructions[i];
   if(in->type!=SWAN_IR_I32&&in->type!=SWAN_IR_I1&&in->type!=SWAN_IR_PTR&&in->type!=SWAN_IR_VOID){if(error&&cap)snprintf(error,cap,"ARMv7 backend supports i32/i1 scalar values only");return false;}
   if(in->op==SWAN_IR_SDIV||in->op==SWAN_IR_UDIV||in->op==SWAN_IR_SREM||in->op==SWAN_IR_UREM){if(error&&cap)snprintf(error,cap,"ARMv7 integer division requires a target runtime helper");return false;}
   if(in->op==SWAN_IR_CALL&&in->incoming_count>4){if(error&&cap)snprintf(error,cap,"ARMv7 calls support at most four register arguments");return false;}
  }
 }
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];size_t allocas=0;
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++)allocas+=fn->blocks[b].instructions[i].op==SWAN_IR_ALLOCA;
  size_t value_bytes=((size_t)fn->next_value+1)*4u,frame=(value_bytes+allocas*4u+7u)&~(size_t)7u;if(frame<8)frame=8;
  text_add(o,".syntax unified\n.arm\n.text\n.align 2\n.global %s\n.type %s, %%function\n%s:\n    push {r4, lr}\n    sub sp, sp, #%zu\n",fn->name,fn->name,fn->name,frame);
  for(size_t p=0;p<fn->parameter_count;p++)text_add(o,"    str r%zu, [sp, #%d]\n",p,swan_armv7_offset((uint32_t)p+1));
  size_t ai=0;
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRBlock *bb=&fn->blocks[b];text_add(o,".Lsw_f%zu_b%zu:\n",f,b);
   for(size_t i=0;i<bb->instruction_count;i++){
    const SwanIRInst *in=&bb->instructions[i];int d=swan_armv7_offset(in->result),a=swan_armv7_offset(in->a),z=swan_armv7_offset(in->b);
    switch(in->op){
     case SWAN_IR_PHI:break;
     case SWAN_IR_CONST:text_add(o,"    ldr r0, =%d\n    str r0, [sp, #%d]\n",(int32_t)in->immediate,d);break;
     case SWAN_IR_ALLOCA:{size_t off=value_bytes+ai++*4u;text_add(o,"    add r0, sp, #%zu\n    str r0, [sp, #%d]\n",off,d);break;}
     case SWAN_IR_LOAD:text_add(o,"    ldr r0, [sp, #%d]\n    ldr r1, [r0]\n    str r1, [sp, #%d]\n",a,d);break;
     case SWAN_IR_STORE:text_add(o,"    ldr r0, [sp, #%d]\n    ldr r1, [sp, #%d]\n    str r1, [r0]\n",a,z);break;
     case SWAN_IR_CALL:for(size_t k=0;k<in->incoming_count;k++)text_add(o,"    ldr r%zu, [sp, #%d]\n",k,swan_armv7_offset(in->incoming[k].value));text_add(o,"    bl %s\n",in->symbol);if(in->type!=SWAN_IR_VOID)text_add(o,"    str r0, [sp, #%d]\n",d);break;
     case SWAN_IR_SELECT:text_add(o,"    ldr r0, [sp, #%d]\n    ldr r1, [sp, #%d]\n    ldr r2, [sp, #%d]\n    cmp r0, #0\n    moveq r1, r2\n    str r1, [sp, #%d]\n",a,z,swan_armv7_offset(in->c),d);break;
     case SWAN_IR_ADD:case SWAN_IR_SUB:case SWAN_IR_MUL:case SWAN_IR_AND:case SWAN_IR_OR:case SWAN_IR_XOR:case SWAN_IR_SHL:case SWAN_IR_LSHR:case SWAN_IR_ASHR:
      text_add(o,"    ldr r2, [sp, #%d]\n    ldr r3, [sp, #%d]\n    %s r1, r2, r3\n    str r1, [sp, #%d]\n",a,z,in->op==SWAN_IR_ADD?"add":in->op==SWAN_IR_SUB?"sub":in->op==SWAN_IR_MUL?"mul":in->op==SWAN_IR_AND?"and":in->op==SWAN_IR_OR?"orr":in->op==SWAN_IR_XOR?"eor":in->op==SWAN_IR_SHL?"lsl":"lsr",d);break;
     case SWAN_IR_ICMP_EQ:case SWAN_IR_ICMP_NE:case SWAN_IR_ICMP_SLT:case SWAN_IR_ICMP_SLE:case SWAN_IR_ICMP_SGT:case SWAN_IR_ICMP_SGE:
      text_add(o,"    ldr r2, [sp, #%d]\n    ldr r3, [sp, #%d]\n    cmp r2, r3\n    mov r1, #0\n    mov%s r1, #1\n    str r1, [sp, #%d]\n",a,z,in->op==SWAN_IR_ICMP_EQ?"eq":in->op==SWAN_IR_ICMP_NE?"ne":in->op==SWAN_IR_ICMP_SLT?"lt":in->op==SWAN_IR_ICMP_SLE?"le":in->op==SWAN_IR_ICMP_SGT?"gt":"ge",d);break;
     default:if(error&&cap)snprintf(error,cap,"ARMv7 backend encountered unsupported IR opcode %u",(unsigned)in->op);return false;
    }
   }
   const SwanIRTerm *t=&bb->terminator;
   if(t->kind==SWAN_IR_RET){if(t->value)text_add(o,"    ldr r0, [sp, #%d]\n",swan_armv7_offset(t->value));else text_add(o,"    mov r0, #0\n");text_add(o,"    add sp, sp, #%zu\n    pop {r4, pc}\n",frame);}
   else if(t->kind==SWAN_IR_BR)text_add(o,"    b .Lsw_e%zu_%zu_%u\n",f,b,t->target);
   else if(t->kind==SWAN_IR_COND_BR)text_add(o,"    ldr r0, [sp, #%d]\n    cmp r0, #0\n    bne .Lsw_e%zu_%zu_%u\n    b .Lsw_e%zu_%zu_%u\n",swan_armv7_offset(t->value),f,b,t->target,f,b,t->alternate);
   else text_add(o,"    udf #0\n");
  }
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRTerm *t=&fn->blocks[b].terminator;size_t targets[2],n=0;
   if(t->kind==SWAN_IR_BR)targets[n++]=t->target;else if(t->kind==SWAN_IR_COND_BR){targets[n++]=t->target;if(t->alternate!=t->target)targets[n++]=t->alternate;}
   for(size_t k=0;k<n;k++){size_t target=targets[k];text_add(o,".Lsw_e%zu_%zu_%zu:\n",f,b,target);const SwanIRBlock *dest=&fn->blocks[target];
    for(size_t i=0;i<dest->instruction_count;i++){const SwanIRInst *phi=&dest->instructions[i];if(phi->op!=SWAN_IR_PHI)continue;
     for(size_t q=0;q<phi->incoming_count;q++)if(phi->incoming[q].block==b){text_add(o,"    ldr r0, [sp, #%d]\n    str r0, [sp, #%d]\n",swan_armv7_offset(phi->incoming[q].value),swan_armv7_offset(phi->result));break;}
    }text_add(o,"    b .Lsw_f%zu_b%zu\n",f,target);
   }
  }
  text_add(o,".size %s, .-%s\n",fn->name,fn->name);
 }
 return !o->failed;
}
#endif
