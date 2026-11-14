#ifndef SWAN_CODEGEN_X86_64_IR_H
#define SWAN_CODEGEN_X86_64_IR_H
#include "swan/codegen_x86_64.h"
#include "swan/ir.h"
static inline int swan_x64_slot(uint32_t id){return (int)(id*8u);}
static inline bool swan_codegen_x86_64_ir(const SwanIRModule *m,Text *o,char *error,size_t cap){
 if(error&&cap)error[0]=0;
 if(!m||!o){if(error&&cap)snprintf(error,cap,"x86-64 IR code generation requires a module and output buffer");return false;}
 if(!swan_ir_verify(m,error,cap))return false;
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];
  if(fn->external)continue;
  if(fn->parameter_count>16){if(error&&cap)snprintf(error,cap,"x86-64 IR backend supports at most 16 parameters");return false;}
  if(fn->return_type!=SWAN_IR_VOID&&fn->return_type!=SWAN_IR_I1&&fn->return_type!=SWAN_IR_I32&&fn->return_type!=SWAN_IR_I64){if(error&&cap)snprintf(error,cap,"x86-64 IR backend currently supports i1, i32, and i64 returns");return false;}
  for(size_t p=0;p<fn->parameter_count;p++)if(fn->parameter_types[p]!=SWAN_IR_I1&&fn->parameter_types[p]!=SWAN_IR_I32&&fn->parameter_types[p]!=SWAN_IR_I64){if(error&&cap)snprintf(error,cap,"x86-64 IR backend currently supports i1, i32, and i64 parameters");return false;}
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
   const SwanIRInst *in=&fn->blocks[b].instructions[i];
   if(in->type!=SWAN_IR_VOID&&in->type!=SWAN_IR_I1&&in->type!=SWAN_IR_I32&&in->type!=SWAN_IR_I64&&in->type!=SWAN_IR_PTR){if(error&&cap)snprintf(error,cap,"x86-64 IR backend currently supports i1/i32/i64 scalar values and pointers");return false;}
   if(in->type==SWAN_IR_I32&&in->op!=SWAN_IR_CONST&&in->op!=SWAN_IR_CALL){if(error&&cap)snprintf(error,cap,"x86-64 C ABI currently supports i32 only at call boundaries");return false;}
   if(in->op==SWAN_IR_CALL&&in->incoming_count>16){if(error&&cap)snprintf(error,cap,"x86-64 IR calls support at most 16 arguments");return false;}
  }
 }
 static const char *args[]={"rdi","rsi","rdx","rcx","r8","r9"};
 for(size_t f=0;f<m->function_count;f++){
  const SwanIRFunction *fn=&m->functions[f];if(fn->external){text_add(o,".extern %s\n",fn->name);continue;}
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
   if(error&&cap)snprintf(error,cap,"x86-64 IR stack frame size overflow");
   return false;
  }
  size_t frame=(value_bytes+allocas*8u+phi_slots*8u+15u)&~(size_t)15u;
  text_add(o,".intel_syntax noprefix\n.text\n.globl %s\n.type %s, @function\n%s:\n    push rbp\n    mov rbp, rsp\n    sub rsp, %zu\n",fn->name,fn->name,fn->name,frame);
  for(size_t p=0;p<fn->parameter_count&&p<6;p++)text_add(o,"    mov QWORD PTR [rbp-%d], %s\n",swan_x64_slot((uint32_t)p+1),args[p]);
  for(size_t p=6;p<fn->parameter_count;p++)text_add(o,"    mov rax, QWORD PTR [rbp+%zu]\n    mov QWORD PTR [rbp-%d], rax\n",16u+(p-6u)*8u,swan_x64_slot((uint32_t)p+1));
  size_t ai=0;
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRBlock *bb=&fn->blocks[b];text_add(o,".Lsw_f%zu_b%zu:\n",f,b);
   for(size_t i=0;i<bb->instruction_count;i++){
    const SwanIRInst *in=&bb->instructions[i];int d=swan_x64_slot(in->result),a=swan_x64_slot(in->a),z=swan_x64_slot(in->b);
    switch(in->op){
     case SWAN_IR_PHI:break;
     case SWAN_IR_CONST:text_add(o,"    mov rax, %lld\n    mov QWORD PTR [rbp-%d], rax\n",(long long)in->immediate,d);break;
     case SWAN_IR_ALLOCA:{size_t off=value_bytes+ai++*8u;text_add(o,"    lea rax, [rbp-%zu]\n    mov QWORD PTR [rbp-%d], rax\n",off,d);break;}
     case SWAN_IR_LOAD:text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov r10, QWORD PTR [rax]\n    mov QWORD PTR [rbp-%d], r10\n",a,d);break;
     case SWAN_IR_STORE:text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov r10, QWORD PTR [rbp-%d]\n    mov QWORD PTR [rax], r10\n",a,z);break;
     case SWAN_IR_CALL:{
      size_t stack_count=in->incoming_count>6?in->incoming_count-6:0,outgoing=(stack_count*8u+15u)&~(size_t)15u;
      if(outgoing)text_add(o,"    sub rsp, %zu\n",outgoing);
      for(size_t k=6;k<in->incoming_count;k++)text_add(o,"    mov r10, QWORD PTR [rbp-%d]\n    mov QWORD PTR [rsp+%zu], r10\n",swan_x64_slot(in->incoming[k].value),(k-6u)*8u);
      for(size_t k=0;k<in->incoming_count&&k<6;k++)text_add(o,"    mov %s, QWORD PTR [rbp-%d]\n",args[k],swan_x64_slot(in->incoming[k].value));
      text_add(o,"    xor eax, eax\n    call %s\n",in->symbol);
      if(outgoing)text_add(o,"    add rsp, %zu\n",outgoing);
      if(in->type==SWAN_IR_I32)text_add(o,"    movsxd rax, eax\n");else if(in->type==SWAN_IR_I1)text_add(o,"    movzx eax, al\n");
      if(in->type!=SWAN_IR_VOID)text_add(o,"    mov QWORD PTR [rbp-%d], rax\n",d);
      break;}
     case SWAN_IR_SELECT:text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    mov r10, QWORD PTR [rbp-%d]\n    test rax, rax\n    cmovne r10, rcx\n    mov QWORD PTR [rbp-%d], r10\n",a,z,swan_x64_slot(in->c),d);break;
     case SWAN_IR_ADD:case SWAN_IR_SUB:case SWAN_IR_MUL:case SWAN_IR_AND:case SWAN_IR_OR:case SWAN_IR_XOR:
      text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    %s rax, rcx\n    mov QWORD PTR [rbp-%d], rax\n",a,z,in->op==SWAN_IR_ADD?"add":in->op==SWAN_IR_SUB?"sub":in->op==SWAN_IR_MUL?"imul":in->op==SWAN_IR_AND?"and":in->op==SWAN_IR_OR?"or":"xor",d);break;
     case SWAN_IR_SDIV:case SWAN_IR_SREM:
      text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    cqo\n    idiv rcx\n    mov QWORD PTR [rbp-%d], %s\n",a,z,d,in->op==SWAN_IR_SDIV?"rax":"rdx");break;
     case SWAN_IR_UDIV:case SWAN_IR_UREM:
      text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    xor edx, edx\n    div rcx\n    mov QWORD PTR [rbp-%d], %s\n",a,z,d,in->op==SWAN_IR_UDIV?"rax":"rdx");break;
     case SWAN_IR_SHL:case SWAN_IR_LSHR:case SWAN_IR_ASHR:
      text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    %s rax, cl\n    mov QWORD PTR [rbp-%d], rax\n",a,z,in->op==SWAN_IR_SHL?"shl":in->op==SWAN_IR_LSHR?"shr":"sar",d);break;
     case SWAN_IR_ICMP_EQ:case SWAN_IR_ICMP_NE:case SWAN_IR_ICMP_SLT:case SWAN_IR_ICMP_SLE:case SWAN_IR_ICMP_SGT:case SWAN_IR_ICMP_SGE:
      text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    cmp rax, rcx\n    set%s al\n    movzx eax, al\n    mov QWORD PTR [rbp-%d], rax\n",a,z,in->op==SWAN_IR_ICMP_EQ?"e":in->op==SWAN_IR_ICMP_NE?"ne":in->op==SWAN_IR_ICMP_SLT?"l":in->op==SWAN_IR_ICMP_SLE?"le":in->op==SWAN_IR_ICMP_SGT?"g":"ge",d);break;
     default:if(error&&cap)snprintf(error,cap,"x86-64 backend encountered unsupported IR opcode %u",(unsigned)in->op);return false;
    }
   }
   const SwanIRTerm *t=&bb->terminator;
   if(t->kind==SWAN_IR_RET){if(t->value)text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n",swan_x64_slot(t->value));else text_add(o,"    xor eax, eax\n");text_add(o,"    mov rsp, rbp\n    pop rbp\n    ret\n");}
   else if(t->kind==SWAN_IR_BR)text_add(o,"    jmp .Lsw_e%zu_%zu_%u\n",f,b,t->target);
   else if(t->kind==SWAN_IR_COND_BR)text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    test rax, rax\n    jne .Lsw_e%zu_%zu_%u\n    jmp .Lsw_e%zu_%zu_%u\n",swan_x64_slot(t->value),f,b,t->target,f,b,t->alternate);
   else text_add(o,"    ud2\n");
  }
  for(size_t b=0;b<fn->block_count;b++){
   const SwanIRTerm *t=&fn->blocks[b].terminator;size_t targets[2],n=0;
   if(t->kind==SWAN_IR_BR)targets[n++]=t->target;else if(t->kind==SWAN_IR_COND_BR){targets[n++]=t->target;if(t->alternate!=t->target)targets[n++]=t->alternate;}
   for(size_t k=0;k<n;k++){size_t target=targets[k];text_add(o,".Lsw_e%zu_%zu_%zu:\n",f,b,target);const SwanIRBlock *dest=&fn->blocks[target];
    size_t phi_index=0;
    for(size_t i=0;i<dest->instruction_count;i++){
     const SwanIRInst *phi=&dest->instructions[i];
     if(phi->op!=SWAN_IR_PHI)continue;
     for(size_t q=0;q<phi->incoming_count;q++)if(phi->incoming[q].block==b){
      size_t scratch_offset=value_bytes+allocas*8u+(phi_index+1u)*8u;
      text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov QWORD PTR [rbp-%zu], rax\n",
               swan_x64_slot(phi->incoming[q].value),scratch_offset);
      break;
     }
     phi_index++;
    }
    phi_index=0;
    for(size_t i=0;i<dest->instruction_count;i++){
     const SwanIRInst *phi=&dest->instructions[i];
     if(phi->op!=SWAN_IR_PHI)continue;
     for(size_t q=0;q<phi->incoming_count;q++)if(phi->incoming[q].block==b){
      size_t scratch_offset=value_bytes+allocas*8u+(phi_index+1u)*8u;
      text_add(o,"    mov rax, QWORD PTR [rbp-%zu]\n    mov QWORD PTR [rbp-%d], rax\n",
               scratch_offset,swan_x64_slot(phi->result));
      break;
     }
     phi_index++;
    }
    text_add(o,"    jmp .Lsw_f%zu_b%zu\n",f,target);
   }
  }
  text_add(o,".size %s, .-%s\n",fn->name,fn->name);
 }
 text_add(o,".section .note.GNU-stack,\"\",@progbits\n");
 return !o->failed;
}
#endif
