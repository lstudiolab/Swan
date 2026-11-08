#ifndef SWAN_CODEGEN_RISCV64_H
#define SWAN_CODEGEN_RISCV64_H
#include "swan/codegen_x86_64.h"
#include "swan/ir.h"
static inline int swan_rv_offset(uint32_t id){return (int)(id*8u);}
static inline bool swan_codegen_riscv64_ir(const SwanIRModule *m,Text *o,char *error,size_t cap){
 if(!swan_ir_verify(m,error,cap))return false;
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  if(fn->parameter_count>16){if(error&&cap)snprintf(error,cap,"RISC-V64 backend supports at most 16 parameters");return false;}
  if(fn->return_type!=SWAN_IR_VOID&&fn->return_type!=SWAN_IR_I1&&fn->return_type!=SWAN_IR_I64){if(error&&cap)snprintf(error,cap,"RISC-V64 backend currently supports i1 and i64 return values");return false;}
  for(size_t p=0;p<fn->parameter_count;p++)if(fn->parameter_types[p]!=SWAN_IR_I1&&fn->parameter_types[p]!=SWAN_IR_I64){if(error&&cap)snprintf(error,cap,"RISC-V64 backend currently supports i1 and i64 parameters");return false;}
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
   const SwanIRInst *in=&fn->blocks[b].instructions[i];
   if(in->type!=SWAN_IR_VOID&&in->type!=SWAN_IR_I1&&in->type!=SWAN_IR_I64&&in->type!=SWAN_IR_PTR){if(error&&cap)snprintf(error,cap,"RISC-V64 backend currently supports i1/i64 scalar values and pointers");return false;}
   if(in->op==SWAN_IR_CALL&&in->incoming_count>16){if(error&&cap)snprintf(error,cap,"RISC-V64 calls support at most 16 arguments");return false;}
  }
 }
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];size_t allocas=0;
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++)allocas+=fn->blocks[b].instructions[i].op==SWAN_IR_ALLOCA;
  size_t value_bytes=((size_t)fn->next_value+1)*8u,frame=(value_bytes+allocas*8u+31u)&~(size_t)15u;if(frame<32)frame=32;
  text_add(o,".text\n.p2align 2\n.globl %s\n.type %s, @function\n%s:\n    addi sp, sp, -%zu\n    sd ra, %zu(sp)\n    sd s0, %zu(sp)\n    addi s0, sp, %zu\n",fn->name,fn->name,fn->name,frame,frame-8,frame-16,frame);
  for(size_t p=0;p<fn->parameter_count&&p<8;p++)text_add(o,"    sd a%zu, %d(sp)\n",p,swan_rv_offset((uint32_t)p+1));
  for(size_t p=8;p<fn->parameter_count;p++)text_add(o,"    ld t0, %zu(sp)\n    sd t0, %d(sp)\n",frame+(p-8u)*8u,swan_rv_offset((uint32_t)p+1));
  size_t ai=0;
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRBlock *bb=&fn->blocks[b];text_add(o,".Lsw_f%zu_b%zu:\n",f,b);
   for(size_t i=0;i<bb->instruction_count;i++){
    const SwanIRInst *in=&bb->instructions[i];int d=swan_rv_offset(in->result),a=swan_rv_offset(in->a),z=swan_rv_offset(in->b);
    switch(in->op){
     case SWAN_IR_PHI:break;
     case SWAN_IR_CONST:text_add(o,"    li t0, %lld\n    sd t0, %d(sp)\n",(long long)in->immediate,d);break;
     case SWAN_IR_ALLOCA:{size_t off=value_bytes+ai++*8u;text_add(o,"    addi t0, sp, %zu\n    sd t0, %d(sp)\n",off,d);break;}
     case SWAN_IR_LOAD:text_add(o,"    ld t0, %d(sp)\n    ld t1, 0(t0)\n    sd t1, %d(sp)\n",a,d);break;
     case SWAN_IR_STORE:text_add(o,"    ld t0, %d(sp)\n    ld t1, %d(sp)\n    sd t1, 0(t0)\n",a,z);break;
     case SWAN_IR_CALL:{
      size_t stack_count=in->incoming_count>8?in->incoming_count-8:0,outgoing=(stack_count*8u+15u)&~(size_t)15u;
      if(outgoing)text_add(o,"    addi sp, sp, -%zu\n",outgoing);
      for(size_t k=8;k<in->incoming_count;k++)text_add(o,"    addi t1, s0, -%zu\n    ld t0, %d(t1)\n    sd t0, %zu(sp)\n",frame,swan_rv_offset(in->incoming[k].value),(k-8u)*8u);
      for(size_t k=0;k<in->incoming_count&&k<8;k++)text_add(o,"    addi t1, s0, -%zu\n    ld a%zu, %d(t1)\n",frame,k,swan_rv_offset(in->incoming[k].value));
      text_add(o,"    call %s\n",in->symbol);
      if(outgoing)text_add(o,"    addi sp, sp, %zu\n",outgoing);
      if(in->type!=SWAN_IR_VOID)text_add(o,"    sd a0, %d(sp)\n",d);break;}
     case SWAN_IR_SELECT:text_add(o,"    ld t0, %d(sp)\n    ld t1, %d(sp)\n    ld t2, %d(sp)\n    beqz t0, .Lsw_sel%zu_%zu_%zu\n    sd t1, %d(sp)\n    j .Lsw_sel_end%zu_%zu_%zu\n.Lsw_sel%zu_%zu_%zu:\n    sd t2, %d(sp)\n.Lsw_sel_end%zu_%zu_%zu:\n",a,z,swan_rv_offset(in->c),f,b,i,d,f,b,i,f,b,i,d,f,b,i);break;
     case SWAN_IR_ADD:case SWAN_IR_SUB:case SWAN_IR_MUL:case SWAN_IR_AND:case SWAN_IR_OR:case SWAN_IR_XOR:case SWAN_IR_SHL:case SWAN_IR_LSHR:case SWAN_IR_ASHR:
      text_add(o,"    ld t0, %d(sp)\n    ld t1, %d(sp)\n    %s t2, t0, t1\n    sd t2, %d(sp)\n",a,z,in->op==SWAN_IR_ADD?"add":in->op==SWAN_IR_SUB?"sub":in->op==SWAN_IR_MUL?"mul":in->op==SWAN_IR_AND?"and":in->op==SWAN_IR_OR?"or":in->op==SWAN_IR_XOR?"xor":in->op==SWAN_IR_SHL?"sll":in->op==SWAN_IR_LSHR?"srl":"sra",d);break;
     case SWAN_IR_SDIV:case SWAN_IR_UDIV:case SWAN_IR_SREM:case SWAN_IR_UREM:{
      int id=o->next_label++;
      text_add(o,"    ld t0, %d(sp)\n    ld t1, %d(sp)\n    beqz t1, .Lsw_rv_divzero_%d\n",a,z,id);
      if(in->op==SWAN_IR_SDIV||in->op==SWAN_IR_SREM)
       text_add(o,"    li t2, 0x8000000000000000\n    bne t0, t2, .Lsw_rv_divsafe_%d\n    li t2, -1\n    beq t1, t2, .Lsw_rv_divzero_%d\n.Lsw_rv_divsafe_%d:\n",id,id,id);
      text_add(o,"    %s t2, t0, t1\n",in->op==SWAN_IR_SDIV?"div":in->op==SWAN_IR_UDIV?"divu":in->op==SWAN_IR_SREM?"rem":"remu");
      text_add(o,"    sd t2, %d(sp)\n    j .Lsw_rv_divdone_%d\n.Lsw_rv_divzero_%d:\n    ebreak\n.Lsw_rv_divdone_%d:\n",d,id,id,id);break;
     }
     case SWAN_IR_ICMP_EQ:case SWAN_IR_ICMP_NE:case SWAN_IR_ICMP_SLT:case SWAN_IR_ICMP_SLE:case SWAN_IR_ICMP_SGT:case SWAN_IR_ICMP_SGE:
      text_add(o,"    ld t0, %d(sp)\n    ld t1, %d(sp)\n",a,z);
      if(in->op==SWAN_IR_ICMP_SLT)text_add(o,"    slt t2, t0, t1\n");
      else if(in->op==SWAN_IR_ICMP_SGT)text_add(o,"    slt t2, t1, t0\n");
      else if(in->op==SWAN_IR_ICMP_EQ)text_add(o,"    xor t2, t0, t1\n    seqz t2, t2\n");
      else if(in->op==SWAN_IR_ICMP_NE)text_add(o,"    xor t2, t0, t1\n    snez t2, t2\n");
      else if(in->op==SWAN_IR_ICMP_SLE)text_add(o,"    slt t2, t1, t0\n    xori t2, t2, 1\n");
      else text_add(o,"    slt t2, t0, t1\n    xori t2, t2, 1\n");
      text_add(o,"    sd t2, %d(sp)\n",d);break;
     default:if(error&&cap)snprintf(error,cap,"RISC-V64 backend encountered unsupported IR opcode %u",(unsigned)in->op);return false;
    }
   }
   const SwanIRTerm *t=&bb->terminator;
   if(t->kind==SWAN_IR_RET){if(t->value)text_add(o,"    ld a0, %d(sp)\n",swan_rv_offset(t->value));else text_add(o,"    li a0, 0\n");text_add(o,"    ld ra, %zu(sp)\n    ld s0, %zu(sp)\n    addi sp, sp, %zu\n    ret\n",frame-8,frame-16,frame);}
   else if(t->kind==SWAN_IR_BR)text_add(o,"    j .Lsw_e%zu_%zu_%u\n",f,b,t->target);
   else if(t->kind==SWAN_IR_COND_BR)text_add(o,"    ld t0, %d(sp)\n    bnez t0, .Lsw_e%zu_%zu_%u\n    j .Lsw_e%zu_%zu_%u\n",swan_rv_offset(t->value),f,b,t->target,f,b,t->alternate);
   else text_add(o,"    ebreak\n");
  }
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRTerm *t=&fn->blocks[b].terminator;size_t targets[2],n=0;
   if(t->kind==SWAN_IR_BR)targets[n++]=t->target;else if(t->kind==SWAN_IR_COND_BR){targets[n++]=t->target;if(t->alternate!=t->target)targets[n++]=t->alternate;}
   for(size_t k=0;k<n;k++){size_t target=targets[k];text_add(o,".Lsw_e%zu_%zu_%zu:\n",f,b,target);const SwanIRBlock *dest=&fn->blocks[target];
    for(size_t i=0;i<dest->instruction_count;i++){const SwanIRInst *phi=&dest->instructions[i];if(phi->op!=SWAN_IR_PHI)continue;
     for(size_t q=0;q<phi->incoming_count;q++)if(phi->incoming[q].block==b){text_add(o,"    ld t0, %d(sp)\n    sd t0, %d(sp)\n",swan_rv_offset(phi->incoming[q].value),swan_rv_offset(phi->result));break;}
    }text_add(o,"    j .Lsw_f%zu_b%zu\n",f,target);
   }
  }
  text_add(o,".size %s, .-%s\n",fn->name,fn->name);
 }
 return !o->failed;
}
#endif
