#ifndef BREW_CODEGEN_X86_64_IR_H
#define BREW_CODEGEN_X86_64_IR_H
#include "brew/codegen_x86_64.h"
#include "brew/ir.h"
static inline int brew_x64_slot(uint32_t id){return (int)(id*8u);}
static inline bool brew_codegen_x86_64_ir(const BrewIRModule *m,Text *o,char *error,size_t cap){
 if(error&&cap)error[0]=0;
 if(!m||!o){if(error&&cap)snprintf(error,cap,"x86-64 IR code generation requires a module and output buffer");return false;}
 if(!brew_ir_verify(m,error,cap))return false;
 for(size_t f=0;f<m->function_count;f++){
  const BrewIRFunction *fn=&m->functions[f];
  if(fn->external)continue;
  if(fn->return_type!=BREW_IR_VOID&&fn->return_type!=BREW_IR_I1&&fn->return_type!=BREW_IR_I32&&fn->return_type!=BREW_IR_I64){if(error&&cap)snprintf(error,cap,"x86-64 IR backend currently supports i1, i32, and i64 returns");return false;}
  for(size_t p=0;p<fn->parameter_count;p++)if(fn->parameter_types[p]!=BREW_IR_I1&&fn->parameter_types[p]!=BREW_IR_I32&&fn->parameter_types[p]!=BREW_IR_I64){if(error&&cap)snprintf(error,cap,"x86-64 IR backend currently supports i1, i32, and i64 parameters");return false;}
  for(size_t b=0;b<fn->block_count;b++)for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
   const BrewIRInst *in=&fn->blocks[b].instructions[i];
   if(in->type!=BREW_IR_VOID&&in->type!=BREW_IR_I1&&in->type!=BREW_IR_I32&&in->type!=BREW_IR_I64&&in->type!=BREW_IR_PTR){if(error&&cap)snprintf(error,cap,"x86-64 IR backend currently supports i1/i32/i64 scalar values and pointers");return false;}
  }
 }
 static const char *args[]={"rdi","rsi","rdx","rcx","r8","r9"};
 for(size_t f=0;f<m->function_count;f++){
  const BrewIRFunction *fn=&m->functions[f];if(fn->external){text_add(o,".extern %s\n",fn->name);continue;}
  size_t allocas=0,phi_slots=0;
  for(size_t b=0;b<fn->block_count;b++){
   size_t block_phi_count=0;
   for(size_t i=0;i<fn->blocks[b].instruction_count;i++){
    const BrewIRInst *instruction=&fn->blocks[b].instructions[i];
    allocas+=instruction->op==BREW_IR_ALLOCA;
    block_phi_count+=instruction->op==BREW_IR_PHI;
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
  if(frame>INT32_MAX){if(error&&cap)snprintf(error,cap,"x86-64 IR stack frame exceeds the signed displacement limit");return false;}
  text_add(o,".intel_syntax noprefix\n.text\n.globl %s\n.type %s, @function\n%s:\n    push rbp\n    mov rbp, rsp\n    sub rsp, %zu\n",fn->name,fn->name,fn->name,frame);
  static const char *arg_bytes[]={"dil","sil","dl","cl","r8b","r9b"};
  static const char *arg_dwords[]={"edi","esi","edx","ecx","r8d","r9d"};
  for(size_t p=0;p<fn->parameter_count&&p<6;p++){
   if(fn->parameter_types[p]==BREW_IR_I1)
    text_add(o,"    movzx eax, %s\n    mov QWORD PTR [rbp-%d], rax\n",arg_bytes[p],brew_x64_slot((uint32_t)p+1));
   else if(fn->parameter_types[p]==BREW_IR_I32)
    text_add(o,"    movsxd rax, %s\n    mov QWORD PTR [rbp-%d], rax\n",arg_dwords[p],brew_x64_slot((uint32_t)p+1));
   else
    text_add(o,"    mov QWORD PTR [rbp-%d], %s\n",brew_x64_slot((uint32_t)p+1),args[p]);
  }
  for(size_t p=6;p<fn->parameter_count;p++){
   size_t source_offset=16u+(p-6u)*8u;
   if(fn->parameter_types[p]==BREW_IR_I1)
    text_add(o,"    movzx eax, BYTE PTR [rbp+%zu]\n    mov QWORD PTR [rbp-%d], rax\n",source_offset,brew_x64_slot((uint32_t)p+1));
   else if(fn->parameter_types[p]==BREW_IR_I32)
    text_add(o,"    movsxd rax, DWORD PTR [rbp+%zu]\n    mov QWORD PTR [rbp-%d], rax\n",source_offset,brew_x64_slot((uint32_t)p+1));
   else
    text_add(o,"    mov rax, QWORD PTR [rbp+%zu]\n    mov QWORD PTR [rbp-%d], rax\n",source_offset,brew_x64_slot((uint32_t)p+1));
  }
  size_t ai=0;
  for(size_t b=0;b<fn->block_count;b++){
   const BrewIRBlock *bb=&fn->blocks[b];text_add(o,".Lsw_f%zu_b%zu:\n",f,b);
   for(size_t i=0;i<bb->instruction_count;i++){
    const BrewIRInst *in=&bb->instructions[i];int d=brew_x64_slot(in->result),a=brew_x64_slot(in->a),z=brew_x64_slot(in->b);
    switch(in->op){
     case BREW_IR_PHI:break;
     case BREW_IR_CONST:text_add(o,"    mov rax, %lld\n    mov QWORD PTR [rbp-%d], rax\n",(long long)in->immediate,d);break;
     case BREW_IR_ALLOCA:{size_t off=value_bytes+ai++*8u;text_add(o,"    lea rax, [rbp-%zu]\n    mov QWORD PTR [rbp-%d], rax\n",off,d);break;}
     case BREW_IR_LOAD:text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov r10, QWORD PTR [rax]\n    mov QWORD PTR [rbp-%d], r10\n",a,d);break;
     case BREW_IR_STORE:text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov r10, QWORD PTR [rbp-%d]\n    mov QWORD PTR [rax], r10\n",a,z);break;
     case BREW_IR_CALL:{
      size_t stack_count=in->incoming_count>6?in->incoming_count-6:0,outgoing=(stack_count*8u+15u)&~(size_t)15u;
      if(outgoing)text_add(o,"    sub rsp, %zu\n",outgoing);
      for(size_t k=6;k<in->incoming_count;k++)text_add(o,"    mov r10, QWORD PTR [rbp-%d]\n    mov QWORD PTR [rsp+%zu], r10\n",brew_x64_slot(in->incoming[k].value),(k-6u)*8u);
      for(size_t k=0;k<in->incoming_count&&k<6;k++)text_add(o,"    mov %s, QWORD PTR [rbp-%d]\n",args[k],brew_x64_slot(in->incoming[k].value));
      text_add(o,"    xor eax, eax\n    call %s\n",in->symbol);
      if(outgoing)text_add(o,"    add rsp, %zu\n",outgoing);
      if(in->type==BREW_IR_I32)text_add(o,"    movsxd rax, eax\n");else if(in->type==BREW_IR_I1)text_add(o,"    movzx eax, al\n");
      if(in->type!=BREW_IR_VOID)text_add(o,"    mov QWORD PTR [rbp-%d], rax\n",d);
      break;}
     case BREW_IR_SELECT:text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    mov r10, QWORD PTR [rbp-%d]\n    test rax, rax\n    cmovne r10, rcx\n    mov QWORD PTR [rbp-%d], r10\n",a,z,brew_x64_slot(in->c),d);break;
     case BREW_IR_ADD:case BREW_IR_SUB:case BREW_IR_MUL:case BREW_IR_AND:case BREW_IR_OR:case BREW_IR_XOR:
      if(in->type==BREW_IR_I32)
       text_add(o,"    mov eax, DWORD PTR [rbp-%d]\n    mov ecx, DWORD PTR [rbp-%d]\n    %s eax, ecx\n    movsxd rax, eax\n    mov QWORD PTR [rbp-%d], rax\n",a,z,in->op==BREW_IR_ADD?"add":in->op==BREW_IR_SUB?"sub":in->op==BREW_IR_MUL?"imul":in->op==BREW_IR_AND?"and":in->op==BREW_IR_OR?"or":"xor",d);
      else
       text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    %s rax, rcx\n    mov QWORD PTR [rbp-%d], rax\n",a,z,in->op==BREW_IR_ADD?"add":in->op==BREW_IR_SUB?"sub":in->op==BREW_IR_MUL?"imul":in->op==BREW_IR_AND?"and":in->op==BREW_IR_OR?"or":"xor",d);
      break;
     case BREW_IR_SDIV:case BREW_IR_SREM:{
      if(in->type==BREW_IR_I32)
       text_add(o,"    mov eax, DWORD PTR [rbp-%d]\n    mov ecx, DWORD PTR [rbp-%d]\n    test ecx, ecx\n    jz .Lsw_x64_divtrap32_%zu_%zu_%zu\n    cmp eax, -2147483648\n    jne .Lsw_x64_divsafe32_%zu_%zu_%zu\n    cmp ecx, -1\n    je .Lsw_x64_divtrap32_%zu_%zu_%zu\n.Lsw_x64_divsafe32_%zu_%zu_%zu:\n    cdq\n    idiv ecx\n    movsxd rax, %s\n    mov QWORD PTR [rbp-%d], rax\n    jmp .Lsw_x64_divdone32_%zu_%zu_%zu\n.Lsw_x64_divtrap32_%zu_%zu_%zu:\n    ud2\n.Lsw_x64_divdone32_%zu_%zu_%zu:\n",
          a,z,f,b,i,f,b,i,f,b,i,f,b,i,in->op==BREW_IR_SDIV?"eax":"edx",d,f,b,i,f,b,i,f,b,i);
      else
       text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    test rcx, rcx\n    jz .Lsw_x64_divtrap_%zu_%zu_%zu\n    movabs r10, -9223372036854775808\n    cmp rax, r10\n    jne .Lsw_x64_divsafe_%zu_%zu_%zu\n    cmp rcx, -1\n    je .Lsw_x64_divtrap_%zu_%zu_%zu\n.Lsw_x64_divsafe_%zu_%zu_%zu:\n    cqo\n    idiv rcx\n    mov QWORD PTR [rbp-%d], %s\n    jmp .Lsw_x64_divdone_%zu_%zu_%zu\n.Lsw_x64_divtrap_%zu_%zu_%zu:\n    ud2\n.Lsw_x64_divdone_%zu_%zu_%zu:\n",
          a,z,f,b,i,f,b,i,f,b,i,f,b,i,d,in->op==BREW_IR_SDIV?"rax":"rdx",f,b,i,f,b,i,f,b,i);
      break;}
     case BREW_IR_UDIV:case BREW_IR_UREM:{
      if(in->type==BREW_IR_I32)
       text_add(o,"    mov eax, DWORD PTR [rbp-%d]\n    mov ecx, DWORD PTR [rbp-%d]\n    test ecx, ecx\n    jz .Lsw_x64_udivtrap32_%zu_%zu_%zu\n    xor edx, edx\n    div ecx\n    %s\n    mov QWORD PTR [rbp-%d], rax\n    jmp .Lsw_x64_udivdone32_%zu_%zu_%zu\n.Lsw_x64_udivtrap32_%zu_%zu_%zu:\n    ud2\n.Lsw_x64_udivdone32_%zu_%zu_%zu:\n",
          a,z,f,b,i,in->op==BREW_IR_UDIV?"movsxd rax, eax":"movsxd rax, edx",d,f,b,i,f,b,i,f,b,i);
      else
       text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    test rcx, rcx\n    jz .Lsw_x64_divtrap_%zu_%zu_%zu\n    xor edx, edx\n    div rcx\n    mov QWORD PTR [rbp-%d], %s\n    jmp .Lsw_x64_divdone_%zu_%zu_%zu\n.Lsw_x64_divtrap_%zu_%zu_%zu:\n    ud2\n.Lsw_x64_divdone_%zu_%zu_%zu:\n",
          a,z,f,b,i,d,in->op==BREW_IR_UDIV?"rax":"rdx",f,b,i,f,b,i,f,b,i);
      break;}
     case BREW_IR_SHL:case BREW_IR_LSHR:case BREW_IR_ASHR:
      if(in->type==BREW_IR_I32)
       text_add(o,"    mov eax, DWORD PTR [rbp-%d]\n    mov ecx, DWORD PTR [rbp-%d]\n    %s eax, cl\n    movsxd rax, eax\n    mov QWORD PTR [rbp-%d], rax\n",a,z,in->op==BREW_IR_SHL?"shl":in->op==BREW_IR_LSHR?"shr":"sar",d);
      else
       text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    %s rax, cl\n    mov QWORD PTR [rbp-%d], rax\n",a,z,in->op==BREW_IR_SHL?"shl":in->op==BREW_IR_LSHR?"shr":"sar",d);
      break;
     case BREW_IR_ICMP_EQ:case BREW_IR_ICMP_NE:case BREW_IR_ICMP_SLT:case BREW_IR_ICMP_SLE:case BREW_IR_ICMP_SGT:case BREW_IR_ICMP_SGE:
      text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov rcx, QWORD PTR [rbp-%d]\n    cmp rax, rcx\n    set%s al\n    movzx eax, al\n    mov QWORD PTR [rbp-%d], rax\n",a,z,in->op==BREW_IR_ICMP_EQ?"e":in->op==BREW_IR_ICMP_NE?"ne":in->op==BREW_IR_ICMP_SLT?"l":in->op==BREW_IR_ICMP_SLE?"le":in->op==BREW_IR_ICMP_SGT?"g":"ge",d);break;
     default:if(error&&cap)snprintf(error,cap,"x86-64 backend encountered unsupported IR opcode %u",(unsigned)in->op);return false;
    }
   }
   const BrewIRTerm *t=&bb->terminator;
   if(t->kind==BREW_IR_RET){if(t->value)text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n",brew_x64_slot(t->value));else text_add(o,"    xor eax, eax\n");text_add(o,"    mov rsp, rbp\n    pop rbp\n    ret\n");}
   else if(t->kind==BREW_IR_BR)text_add(o,"    jmp .Lsw_e%zu_%zu_%u\n",f,b,t->target);
   else if(t->kind==BREW_IR_COND_BR)text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    test rax, rax\n    jne .Lsw_e%zu_%zu_%u\n    jmp .Lsw_e%zu_%zu_%u\n",brew_x64_slot(t->value),f,b,t->target,f,b,t->alternate);
   else text_add(o,"    ud2\n");
  }
  for(size_t b=0;b<fn->block_count;b++){
   const BrewIRTerm *t=&fn->blocks[b].terminator;size_t targets[2],n=0;
   if(t->kind==BREW_IR_BR)targets[n++]=t->target;else if(t->kind==BREW_IR_COND_BR){targets[n++]=t->target;if(t->alternate!=t->target)targets[n++]=t->alternate;}
   for(size_t k=0;k<n;k++){size_t target=targets[k];text_add(o,".Lsw_e%zu_%zu_%zu:\n",f,b,target);const BrewIRBlock *dest=&fn->blocks[target];
    size_t phi_index=0;
    for(size_t i=0;i<dest->instruction_count;i++){
     const BrewIRInst *phi=&dest->instructions[i];
     if(phi->op!=BREW_IR_PHI)continue;
     for(size_t q=0;q<phi->incoming_count;q++)if(phi->incoming[q].block==b){
      size_t scratch_offset=value_bytes+allocas*8u+phi_index*8u;
      text_add(o,"    mov rax, QWORD PTR [rbp-%d]\n    mov QWORD PTR [rbp-%zu], rax\n",
               brew_x64_slot(phi->incoming[q].value),scratch_offset);
      break;
     }
     phi_index++;
    }
    phi_index=0;
    for(size_t i=0;i<dest->instruction_count;i++){
     const BrewIRInst *phi=&dest->instructions[i];
     if(phi->op!=BREW_IR_PHI)continue;
     for(size_t q=0;q<phi->incoming_count;q++)if(phi->incoming[q].block==b){
      size_t scratch_offset=value_bytes+allocas*8u+phi_index*8u;
      text_add(o,"    mov rax, QWORD PTR [rbp-%zu]\n    mov QWORD PTR [rbp-%d], rax\n",
               scratch_offset,brew_x64_slot(phi->result));
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
