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
 if(error&&cap)error[0]=0;
 if(!m||!o){if(error&&cap)snprintf(error,cap,"AArch64 code generation requires a module and output buffer");return false;}
 if(!swan_ir_verify(m,error,cap))return false;
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  if(fn->parameter_count>16){if(error&&cap)snprintf(error,cap,"AArch64 backend supports at most 16 parameters");return false;}
  if(fn->return_type!=SWAN_IR_VOID&&fn->return_type!=SWAN_IR_I1&&fn->return_type!=SWAN_IR_I64){if(error&&cap)snprintf(error,cap,"AArch64 backend currently supports i1 and i64 return values");return false;}
  for(size_t p=0;p<fn->parameter_count;p++)if(fn->parameter_types[p]!=SWAN_IR_I1&&fn->parameter_types[p]!=SWAN_IR_I64){if(error&&cap)snprintf(error,cap,"AArch64 backend currently supports i1 and i64 parameters");return false;}
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
   const SwanIRInst *in=&fn->blocks[b].instructions[i];
   if(in->type!=SWAN_IR_VOID&&in->type!=SWAN_IR_I1&&in->type!=SWAN_IR_I64&&in->type!=SWAN_IR_PTR){if(error&&cap)snprintf(error,cap,"AArch64 backend currently supports i1/i64 scalar values and pointers");return false;}
   if(in->op==SWAN_IR_CALL&&in->incoming_count>16){if(error&&cap)snprintf(error,cap,"AArch64 calls support at most 16 arguments");return false;}
  }
 }
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  size_t allocas=0,phi_slots=0;
  for(size_t b=0;b<fn->block_count;b++){
   size_t block_phi_count=0;
   for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
    const SwanIRInst *instruction=&fn->blocks[b].instructions[i];
    allocas+=instruction->op==SWAN_IR_ALLOCA;
    block_phi_count+=instruction->op==SWAN_IR_PHI;
   }
   if(block_phi_count>phi_slots)phi_slots=block_phi_count;
  }
  size_t value_bytes=((size_t)fn->next_value+1)*8u;
  if(allocas>(SIZE_MAX-value_bytes)/8u||
     phi_slots>(SIZE_MAX-value_bytes-allocas*8u)/8u||
     value_bytes+allocas*8u+phi_slots*8u>SIZE_MAX-15u){
   if(error&&cap)snprintf(error,cap,"AArch64 stack frame size overflow");
   return false;
  }
  size_t frame=(value_bytes+allocas*8u+phi_slots*8u+15u)&~(size_t)15u;
  if(frame<16)frame=16;
  if(frame>4095){if(error&&cap)snprintf(error,cap,"AArch64 backend stack frame exceeds its immediate-offset limit");return false;}
  text_add(o,".text\n.p2align 2\n.global %s\n.type %s, %%function\n%s:\n    stp x29, x30, [sp, #-16]!\n    mov x29, sp\n    sub sp, sp, #%zu\n",fn->name,fn->name,fn->name,frame);
  for(size_t p=0;p<fn->parameter_count&&p<8;p++){
   if(fn->parameter_types[p]==SWAN_IR_I1)
    text_add(o,"    and x9, x%zu, #1\n    str x9, [sp, #%d]\n",p,swan_a64_value_offset((uint32_t)p+1));
   else text_add(o,"    str x%zu, [sp, #%d]\n",p,swan_a64_value_offset((uint32_t)p+1));
  }
  for(size_t p=8;p<fn->parameter_count;p++){
   size_t offset=16u+(p-8u)*8u;
   if(fn->parameter_types[p]==SWAN_IR_I1)
    text_add(o,"    ldrb w9, [x29, #%zu]\n    str x9, [sp, #%d]\n",offset,swan_a64_value_offset((uint32_t)p+1));
   else text_add(o,"    ldr x9, [x29, #%zu]\n    str x9, [sp, #%d]\n",offset,swan_a64_value_offset((uint32_t)p+1));
  }
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
     case SWAN_IR_CALL:{
      size_t stack_count=in->incoming_count>8?in->incoming_count-8:0,outgoing=(stack_count*8u+15u)&~(size_t)15u;
      if(outgoing)text_add(o,"    sub sp, sp, #%zu\n",outgoing);
      for(size_t k=8;k<in->incoming_count;k++)
       text_add(o,"    sub x10, x29, #%zu\n    ldr x9, [x10, #%d]\n    str x9, [sp, #%zu]\n",
                frame,swan_a64_value_offset(in->incoming[k].value),(k-8u)*8u);
      for(size_t k=0;k<in->incoming_count&&k<8;k++)
       text_add(o,"    sub x10, x29, #%zu\n    ldr x%zu, [x10, #%d]\n",
                frame,k,swan_a64_value_offset(in->incoming[k].value));
      text_add(o,"    bl %s\n",in->symbol);
      if(in->type==SWAN_IR_I1)text_add(o,"    and x0, x0, #1\n");
      if(outgoing)text_add(o,"    add sp, sp, #%zu\n",outgoing);
      if(in->type!=SWAN_IR_VOID)text_add(o,"    str x0, [sp, #%d]\n",dst);
      break;}
     case SWAN_IR_SELECT:text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    ldr x11, [sp, #%d]\n    cmp x9, #0\n    csel x12, x10, x11, ne\n    str x12, [sp, #%d]\n",a,z,swan_a64_value_offset(in->c),dst);break;
     case SWAN_IR_ADD:case SWAN_IR_SUB:case SWAN_IR_MUL:case SWAN_IR_AND:case SWAN_IR_OR:case SWAN_IR_XOR:case SWAN_IR_SHL:case SWAN_IR_LSHR:case SWAN_IR_ASHR:
      text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    %s x11, x9, x10\n    str x11, [sp, #%d]\n",a,z,in->op==SWAN_IR_ADD?"add":in->op==SWAN_IR_SUB?"sub":in->op==SWAN_IR_MUL?"mul":in->op==SWAN_IR_SDIV?"sdiv":in->op==SWAN_IR_UDIV?"udiv":in->op==SWAN_IR_AND?"and":in->op==SWAN_IR_OR?"orr":in->op==SWAN_IR_XOR?"eor":in->op==SWAN_IR_SHL?"lsl":in->op==SWAN_IR_LSHR?"lsr":"asr",dst);break;
     case SWAN_IR_SDIV:case SWAN_IR_UDIV:case SWAN_IR_SREM:case SWAN_IR_UREM:{
      int id=o->next_label++;
      text_add(o,"    ldr x9, [sp, #%d]\n    ldr x10, [sp, #%d]\n    cbz x10, .Lsw_a64_divzero_%d\n",a,z,id);
      if(in->op==SWAN_IR_SDIV||in->op==SWAN_IR_SREM)
       text_add(o,"    ldr x11, =0x8000000000000000\n    cmp x9, x11\n    b.ne .Lsw_a64_divsafe_%d\n    cmn x10, #1\n    b.eq .Lsw_a64_divzero_%d\n.Lsw_a64_divsafe_%d:\n",id,id,id);
      text_add(o,"    %s x11, x9, x10\n",in->op==SWAN_IR_SDIV||in->op==SWAN_IR_SREM?"sdiv":"udiv");
      if(in->op==SWAN_IR_SREM||in->op==SWAN_IR_UREM)text_add(o,"    msub x11, x11, x10, x9\n");
      text_add(o,"    str x11, [sp, #%d]\n    b .Lsw_a64_divdone_%d\n.Lsw_a64_divzero_%d:\n    brk #0\n.Lsw_a64_divdone_%d:\n",dst,id,id,id);break;
     }
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
    size_t phi_index=0;
    for(size_t i=0;i<dest->instruction_count;i++){
     const SwanIRInst *phi=&dest->instructions[i];
     if(phi->op!=SWAN_IR_PHI)continue;
     for(size_t q=0;q<phi->incoming_count;q++)if(phi->incoming[q].block==b){
      size_t scratch=value_bytes+allocas*8u+(phi_index+1u)*8u;
      text_add(o,"    ldr x9, [sp, #%d]\n    str x9, [sp, #%zu]\n",
               swan_a64_value_offset(phi->incoming[q].value),scratch);
      break;
     }
     phi_index++;
    }
    phi_index=0;
    for(size_t i=0;i<dest->instruction_count;i++){
     const SwanIRInst *phi=&dest->instructions[i];
     if(phi->op!=SWAN_IR_PHI)continue;
     for(size_t q=0;q<phi->incoming_count;q++)if(phi->incoming[q].block==b){
      size_t scratch=value_bytes+allocas*8u+(phi_index+1u)*8u;
      text_add(o,"    ldr x9, [sp, #%zu]\n    str x9, [sp, #%d]\n",
               scratch,swan_a64_value_offset(phi->result));
      break;
     }
     phi_index++;
    }
    text_add(o,"    b .Lsw_f%zu_b%zu\n",f,target);
   }
  }
  text_add(o,".size %s, .-%s\n",fn->name,fn->name);
 }
 return !o->failed;
}
#endif
