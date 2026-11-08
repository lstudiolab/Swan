#ifndef SWAN_CODEGEN_RISCV64_H
#define SWAN_CODEGEN_RISCV64_H
#include "swan/codegen_x86_64.h"
#include "swan/ir.h"
static inline int swan_rv_offset(uint32_t id){return (int)(id*8u);}
static inline bool swan_codegen_riscv64_ir(const SwanIRModule *m,Text *o,char *error,size_t cap){
 if(!swan_ir_verify(m,error,cap))return false;
 for(size_t f=0;f<m->function_count;f++)if(m->functions[f].block_count!=1||m->functions[f].blocks[0].terminator.kind!=SWAN_IR_RET||m->functions[f].parameter_count>8){
  if(error&&cap)snprintf(error,cap,"RISC-V64 backend currently requires straight-line functions with at most eight parameters");return false;
 }
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];const SwanIRBlock *bb=&fn->blocks[0];
  int frame=(int)((fn->next_value*8u+31u)&~15u);if(frame<16)frame=16;
  text_add(o,".text\n.p2align 2\n.globl %s\n.type %s, @function\n%s:\n",fn->name,fn->name,fn->name);
  text_add(o,"    addi sp, sp, -%d\n    sd ra, %d(sp)\n    sd s0, %d(sp)\n    addi s0, sp, %d\n",frame,frame-8,frame-16,frame);
  for(size_t p=0;p<fn->parameter_count;p++)text_add(o,"    sd a%zu, %d(sp)\n",p,swan_rv_offset((uint32_t)p+1));
  for(size_t i=0;i<bb->instruction_count;i++){
   const SwanIRInst *in=&bb->instructions[i];int dst=swan_rv_offset(in->result),a=swan_rv_offset(in->a),b=swan_rv_offset(in->b);
   switch(in->op){
    case SWAN_IR_CONST:text_add(o,"    li t0, %lld\n    sd t0, %d(sp)\n",(long long)in->immediate,dst);break;
    case SWAN_IR_CALL:
     if(in->incoming_count>8){if(error&&cap)snprintf(error,cap,"RISC-V64 call has more than eight arguments");return false;}
     for(size_t k=0;k<in->incoming_count;k++)text_add(o,"    ld a%zu, %d(sp)\n",k,swan_rv_offset(in->incoming[k].value));
     text_add(o,"    call %s\n",in->symbol);if(in->type!=SWAN_IR_VOID)text_add(o,"    sd a0, %d(sp)\n",dst);break;
    case SWAN_IR_ADD:case SWAN_IR_SUB:case SWAN_IR_MUL:case SWAN_IR_SDIV:case SWAN_IR_UDIV:case SWAN_IR_SREM:case SWAN_IR_UREM:
    case SWAN_IR_AND:case SWAN_IR_OR:case SWAN_IR_XOR:case SWAN_IR_SHL:case SWAN_IR_LSHR:case SWAN_IR_ASHR:
     text_add(o,"    ld t0, %d(sp)\n    ld t1, %d(sp)\n    %s t2, t0, t1\n    sd t2, %d(sp)\n",a,b,
      in->op==SWAN_IR_ADD?"add":in->op==SWAN_IR_SUB?"sub":in->op==SWAN_IR_MUL?"mul":in->op==SWAN_IR_SDIV?"div":in->op==SWAN_IR_UDIV?"divu":in->op==SWAN_IR_SREM?"rem":in->op==SWAN_IR_UREM?"remu":in->op==SWAN_IR_AND?"and":in->op==SWAN_IR_OR?"or":in->op==SWAN_IR_XOR?"xor":in->op==SWAN_IR_SHL?"sll":in->op==SWAN_IR_LSHR?"srl":"sra",dst);break;
    case SWAN_IR_ICMP_EQ:case SWAN_IR_ICMP_NE:case SWAN_IR_ICMP_SLT:case SWAN_IR_ICMP_SLE:case SWAN_IR_ICMP_SGT:case SWAN_IR_ICMP_SGE:
     text_add(o,"    ld t0, %d(sp)\n    ld t1, %d(sp)\n",a,b);
     if(in->op==SWAN_IR_ICMP_SLT)text_add(o,"    slt t2, t0, t1\n");
     else if(in->op==SWAN_IR_ICMP_SGT)text_add(o,"    slt t2, t1, t0\n");
     else if(in->op==SWAN_IR_ICMP_EQ)text_add(o,"    xor t2, t0, t1\n    seqz t2, t2\n");
     else if(in->op==SWAN_IR_ICMP_NE)text_add(o,"    xor t2, t0, t1\n    snez t2, t2\n");
     else if(in->op==SWAN_IR_ICMP_SLE)text_add(o,"    slt t2, t1, t0\n    xori t2, t2, 1\n");
     else text_add(o,"    slt t2, t0, t1\n    xori t2, t2, 1\n");
     text_add(o,"    sd t2, %d(sp)\n",dst);break;
    default:if(error&&cap)snprintf(error,cap,"RISC-V64 backend encountered unsupported IR instruction");return false;
   }
  }
  if(bb->terminator.value)text_add(o,"    ld a0, %d(sp)\n",swan_rv_offset(bb->terminator.value));else text_add(o,"    li a0, 0\n");
  text_add(o,"    ld ra, %d(sp)\n    ld s0, %d(sp)\n    addi sp, sp, %d\n    ret\n.size %s, .-%s\n",frame-8,frame-16,frame,fn->name,fn->name);
 }
 return !o->failed;
}
#endif
