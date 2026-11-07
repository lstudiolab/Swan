#ifndef SWAN_CODEGEN_X86_64_H
#define SWAN_CODEGEN_X86_64_H
#include "swan/parser.h"
typedef struct { char *data; size_t len,cap; int stack_depth; } Text;
static inline void text_add(Text *t,const char *fmt,...){
 va_list ap;va_start(ap,fmt);va_list cp;va_copy(cp,ap);int n=vsnprintf(NULL,0,fmt,cp);va_end(cp);
 if(n<0){va_end(ap);return;}size_t need=t->len+(size_t)n+1;
 if(need>t->cap){size_t cap=t->cap?t->cap:4096;while(cap<need)cap*=2;char *p=realloc(t->data,cap);if(!p){va_end(ap);return;}t->data=p;t->cap=cap;}
 vsnprintf(t->data+t->len,t->cap-t->len,fmt,ap);t->len+=(size_t)n;va_end(ap);
}
static inline void cg_reserve(Text *o,int n){if(n>0){text_add(o,"    sub rsp, %d\n",n);o->stack_depth+=n;}}
static inline void cg_release(Text *o,int n){if(n>0){text_add(o,"    add rsp, %d\n",n);o->stack_depth-=n;}}
static inline void cg_push(Text *o,const char *reg){text_add(o,"    push %s\n",reg);o->stack_depth+=8;}
static inline void cg_pop(Text *o,const char *reg){text_add(o,"    pop %s\n",reg);o->stack_depth-=8;}
static inline void cg_call(Text *o,const char *symbol){int pad=(16-(o->stack_depth%16))%16;cg_reserve(o,pad);text_add(o,"    call %s\n",symbol);cg_release(o,pad);}
typedef struct { char name[SWAN_MAX_TEXT]; int slot,reg; bool mut; } Local;
typedef struct { Local items[1024];size_t count;int slots,reg_count; } Locals;
static const char *cg_saved_regs[]={"rbx","r12","r13","r14","r15"};
static inline Local *local_find(Locals *l,const char *name){for(size_t i=l->count;i>0;i--)if(!strcmp(l->items[i-1].name,name))return &l->items[i-1];return NULL;}
static inline Local *local_add(Locals *l,const char *name,bool mut){
 if(l->count>=1024)return NULL;
 Local *v=&l->items[l->count++];strcpy(v->name,name);v->mut=mut;v->reg=-1;v->slot=0;
 if(l->reg_count<5)v->reg=l->reg_count++;
 else v->slot=++l->slots;
 return v;
}
static inline void collect_locals(Stmt **body,size_t n,Locals *l){for(size_t i=0;i<n;i++){Stmt *s=body[i];if(s->kind==S_VARIABLE)local_add(l,s->name,s->mutable_value);collect_locals(s->body,s->body_count,l);collect_locals(s->alternate,s->alt_count,l);}}
static inline void emit_restore_regs(Text *o,Locals *l){
 for(int i=0;i<l->reg_count;i++)text_add(o,"    mov %s, QWORD PTR [rbp - %d]\n",cg_saved_regs[i],l->slots*8+(i+1)*8);
}
static inline bool expr_has_call(Expr *e){
 if(!e)return false;if(e->kind==E_CALL)return true;
 if(expr_has_call(e->left)||expr_has_call(e->right))return true;
 for(size_t i=0;i<e->argc;i++)if(expr_has_call(e->args[i]))return true;
 return false;
}
static inline void emit_expr(Text *o,Expr *e,Locals *l,SwanError *err);
static inline void emit_add_terms(Text *o,Expr *e,Locals *l,SwanError *err,bool *first){
 if(e&&e->kind==E_BINARY&&e->token.kind==T_PLUS){
  emit_add_terms(o,e->left,l,err,first);emit_add_terms(o,e->right,l,err,first);return;
 }
 emit_expr(o,e,l,err);
 if(*first){text_add(o,"    mov r10, rax\n");*first=false;}
 else text_add(o,"    add r10, rax\n");
}
static inline bool emit_accumulate_statement(Text *o,Expr *e,Locals *l,SwanError *err){
 if(!e||e->kind!=E_BINARY||e->token.kind!=T_ASSIGN||!e->left||e->left->kind!=E_NAME)return false;
 Expr *rhs=e->right;
 if(!rhs||rhs->kind!=E_BINARY||rhs->token.kind!=T_PLUS||!rhs->left||rhs->left->kind!=E_NAME||strcmp(rhs->left->token.text,e->left->token.text)||expr_has_call(rhs->right))return false;
 Local *v=local_find(l,e->left->token.text);if(!v)return false;
 if(rhs->right->kind==E_INT){
  char *end=NULL;long long imm=strtoll(rhs->right->token.text,&end,10);
  if(end&&!*end&&imm>=INT32_MIN&&imm<=INT32_MAX){
   if(v->reg>=0)text_add(o,"    add %s, %lld\n",cg_saved_regs[v->reg],imm);
   else text_add(o,"    add QWORD PTR [rbp - %d], %lld\n",v->slot*8,imm);
   return true;
  }
 }
 emit_expr(o,rhs->right,l,err);
 if(v->reg>=0)text_add(o,"    add %s, rax\n",cg_saved_regs[v->reg]);
 else text_add(o,"    add QWORD PTR [rbp - %d], rax\n",v->slot*8);
 (void)err;return true;
}

static inline void emit_call(Text *o,Expr *e,Locals *l,SwanError *err){
 const char *name=e->left->token.text;
 if(!strcmp(name,"array")){swan_error(err,&e->token,"array lowering is not ported to the C backend yet");return;}
 if(e->argc>6){swan_error(err,&e->token,"x86-64 backend supports at most six arguments");return;}
 int area=(int)(((e->argc*8+15)/16)*16);
 int pad=(16-((o->stack_depth+area)%16))%16;
 cg_reserve(o,area+pad);
 for(size_t i=0;i<e->argc;i++){emit_expr(o,e->args[i],l,err);text_add(o,"    mov QWORD PTR [rsp + %d], rax\n",pad+(int)i*8);}
 const char *regs[]={"rdi","rsi","rdx","rcx","r8","r9"};
 for(size_t i=0;i<e->argc;i++)text_add(o,"    mov %s, QWORD PTR [rsp + %d]\n",regs[i],pad+(int)i*8);
 char symbol[SWAN_MAX_TEXT+16];if(!strcmp(name,"main"))strcpy(symbol,"main");else snprintf(symbol,sizeof(symbol),"swan_fn_%s",name);
 text_add(o,"    xor eax, eax\n    call %s\n",symbol);cg_release(o,area+pad);
}
static inline void emit_expr(Text *o,Expr *e,Locals *l,SwanError *err){
 if(!e)return;
 switch(e->kind){
 case E_INT:text_add(o,"    mov rax, %s\n",e->token.text);break;
 case E_BOOL:text_add(o,"    mov eax, %d\n",e->token.kind==T_TRUE);break;
 case E_FLOAT:swan_error(err,&e->token,"floating point is not yet supported by the C backend");break;
 case E_STRING:swan_error(err,&e->token,"string values are not yet supported outside log statements");break;
 case E_NAME:{Local *v=local_find(l,e->token.text);if(!v)swan_error(err,&e->token,"unknown variable '%s'",e->token.text);else if(v->reg>=0)text_add(o,"    mov rax, %s\n",cg_saved_regs[v->reg]);else text_add(o,"    mov rax, QWORD PTR [rbp - %d]\n",v->slot*8);break;}
 case E_UNARY:emit_expr(o,e->left,l,err);if(e->token.kind==T_MINUS)text_add(o,"    neg rax\n");else text_add(o,"    test rax, rax\n    sete al\n    movzx rax, al\n");break;
 case E_CALL:emit_call(o,e,l,err);break;
 case E_BINARY:
  if(e->token.kind==T_PLUS&&!expr_has_call(e)){
   bool first=true;emit_add_terms(o,e,l,err,&first);if(!first)text_add(o,"    mov rax, r10\n");break;
  }
  if(e->token.kind==T_ASSIGN){
   if(e->left->kind!=E_NAME){swan_error(err,&e->token,"assignment target must be a variable");break;}
   Local *v=local_find(l,e->left->token.text);
   if(!v){swan_error(err,&e->token,"unknown assignment target");break;}
   emit_expr(o,e->right,l,err);
   if(v->reg>=0)text_add(o,"    mov %s, rax\n",cg_saved_regs[v->reg]);
   else text_add(o,"    mov QWORD PTR [rbp - %d], rax\n",v->slot*8);
   break;
  }
  if(e->right&&e->right->kind==E_INT){
   char *end=NULL;long long imm=strtoll(e->right->token.text,&end,10);
   if(end&&*end==0&&imm>=INT32_MIN&&imm<=INT32_MAX){
    if(e->left&&e->left->kind==E_NAME){
     Local *v=local_find(l,e->left->token.text);
     if(v&&v->reg>=0){
      const char *reg=cg_saved_regs[v->reg];
      if(e->token.kind==T_PLUS||e->token.kind==T_MINUS){
       long long disp=e->token.kind==T_PLUS?imm:-imm;
       if(disp>=INT32_MIN&&disp<=INT32_MAX){
        if(disp>=0)text_add(o,"    lea rax, [%s + %lld]\n",reg,disp);
        else text_add(o,"    lea rax, [%s - %lld]\n",reg,-disp);
        break;
       }
      }
      if(e->token.kind==T_STAR){text_add(o,"    imul rax, %s, %lld\n",reg,imm);break;}
     }
    }
    emit_expr(o,e->left,l,err);
    if(e->token.kind==T_PLUS){text_add(o,"    add rax, %lld\n",imm);break;}
    if(e->token.kind==T_MINUS){text_add(o,"    sub rax, %lld\n",imm);break;}
    if(e->token.kind==T_STAR){text_add(o,"    imul rax, rax, %lld\n",imm);break;}
    if(e->token.kind==T_EQ||e->token.kind==T_NE||e->token.kind==T_LT||e->token.kind==T_GT||e->token.kind==T_LE||e->token.kind==T_GE){
     const char *cc=e->token.kind==T_EQ?"sete":e->token.kind==T_NE?"setne":e->token.kind==T_LT?"setl":e->token.kind==T_GT?"setg":e->token.kind==T_LE?"setle":"setge";
     text_add(o,"    cmp rax, %lld\n    %s al\n    movzx rax, al\n",imm,cc);break;
    }
    if(e->token.kind==T_PERCENT&&imm>1){
     uint64_t divisor=(uint64_t)imm;
     if((divisor&(divisor-1))==0){
      text_add(o,"    mov rcx, rax\n    sar rcx, 63\n    and rcx, %lld\n    add rax, rcx\n    and rax, %lld\n    sub rax, rcx\n",imm-1,imm-1);break;
     }
     uint64_t magic=UINT64_MAX/divisor;
     text_add(o,"    test rax, rax\n    js 1f\n    mov rcx, rax\n    movabs rdx, 0x%llx\n    mul rdx\n    imul rdx, rdx, %lld\n    sub rcx, rdx\n    lea rdx, [rcx - %lld]\n    cmp rcx, %lld\n    cmovae rcx, rdx\n    jmp 2f\n1:\n    mov rcx, %lld\n    cqo\n    idiv rcx\n    mov rcx, rdx\n2:\n    mov rax, rcx\n",
      (unsigned long long)magic,imm,imm,imm,imm);break;
    }
   }
  }
  emit_expr(o,e->left,l,err);cg_push(o,"rax");emit_expr(o,e->right,l,err);text_add(o,"    mov rcx, rax\n");cg_pop(o,"rax");
  switch(e->token.kind){
   case T_PLUS:text_add(o,"    add rax, rcx\n");break;
   case T_MINUS:text_add(o,"    sub rax, rcx\n");break;
   case T_STAR:text_add(o,"    imul rax, rcx\n");break;
   case T_SLASH:case T_FLOORDIV:case T_PERCENT:text_add(o,"    cqo\n    idiv rcx\n");if(e->token.kind==T_PERCENT)text_add(o,"    mov rax, rdx\n");break;
   case T_EQ:case T_NE:case T_LT:case T_GT:case T_LE:case T_GE:{
    const char *cc=e->token.kind==T_EQ?"sete":e->token.kind==T_NE?"setne":e->token.kind==T_LT?"setl":e->token.kind==T_GT?"setg":e->token.kind==T_LE?"setle":"setge";
    text_add(o,"    cmp rax, rcx\n    %s al\n    movzx rax, al\n",cc);break;
   }
   default:swan_error(err,&e->token,"operator not supported by backend");break;
  }
  break;
 }
}
static inline void emit_condition_jump(Text *o,Expr *e,Locals *l,SwanError *err,const char *target,bool when_true){
 if(e&&e->kind==E_UNARY&&e->token.kind==T_NOT){emit_condition_jump(o,e->left,l,err,target,!when_true);return;}
 if(e&&e->kind==E_BINARY){
  const char *cc=NULL;
  switch(e->token.kind){
   case T_EQ:cc=when_true?"je":"jne";break;case T_NE:cc=when_true?"jne":"je";break;
   case T_LT:cc=when_true?"jl":"jge";break;case T_GT:cc=when_true?"jg":"jle";break;
   case T_LE:cc=when_true?"jle":"jg";break;case T_GE:cc=when_true?"jge":"jl";break;default:break;
  }
  if(cc){
   if(e->left&&e->left->kind==E_NAME&&e->right&&e->right->kind==E_INT){
    char *end=NULL;long long imm=strtoll(e->right->token.text,&end,10);Local *v=local_find(l,e->left->token.text);
    if(v&&end&&!*end&&imm>=INT32_MIN&&imm<=INT32_MAX){
     if(v->reg>=0)text_add(o,"    cmp %s, %lld\n    %s %s\n",cg_saved_regs[v->reg],imm,cc,target);
     else text_add(o,"    cmp QWORD PTR [rbp - %d], %lld\n    %s %s\n",v->slot*8,imm,cc,target);
     return;
    }
   }
   emit_expr(o,e->left,l,err);
   if(e->right&&e->right->kind==E_INT){
    char *end=NULL;long long imm=strtoll(e->right->token.text,&end,10);
    if(end&&!*end&&imm>=INT32_MIN&&imm<=INT32_MAX){text_add(o,"    cmp rax, %lld\n    %s %s\n",imm,cc,target);return;}
   }
   cg_push(o,"rax");emit_expr(o,e->right,l,err);text_add(o,"    mov rcx, rax\n");cg_pop(o,"rax");
   text_add(o,"    cmp rax, rcx\n    %s %s\n",cc,target);return;
  }
 }
 emit_expr(o,e,l,err);text_add(o,"    test rax, rax\n    %s %s\n",when_true?"jnz":"jz",target);
}
static inline bool emit_increment_statement(Text *o,Expr *e,Locals *l){
 if(!e||e->kind!=E_BINARY||e->token.kind!=T_ASSIGN||!e->left||e->left->kind!=E_NAME)return false;
 Expr *rhs=e->right;
 if(!rhs||rhs->kind!=E_BINARY||(rhs->token.kind!=T_PLUS&&rhs->token.kind!=T_MINUS)||!rhs->left||rhs->left->kind!=E_NAME||strcmp(rhs->left->token.text,e->left->token.text)||!rhs->right||rhs->right->kind!=E_INT)return false;
 char *end=NULL;long long imm=strtoll(rhs->right->token.text,&end,10);
 if(!end||*end||imm<INT32_MIN||imm>INT32_MAX)return false;
 if(rhs->token.kind==T_MINUS){if(imm==INT32_MIN)return false;imm=-imm;}
 Local *v=local_find(l,e->left->token.text);if(!v)return false;
 if(v->reg>=0)text_add(o,"    add %s, %lld\n",cg_saved_regs[v->reg],imm);
 else text_add(o,"    add QWORD PTR [rbp - %d], %lld\n",v->slot*8,imm);
 return true;
}
static inline bool expr_writes_name(Expr *e,const char *name){
 if(!e)return false;
 if(e->kind==E_BINARY&&e->token.kind==T_ASSIGN&&e->left&&e->left->kind==E_NAME&&!strcmp(e->left->token.text,name))return true;
 if(expr_writes_name(e->left,name)||expr_writes_name(e->right,name))return true;
 for(size_t i=0;i<e->argc;i++)if(expr_writes_name(e->args[i],name))return true;
 return false;
}
static inline bool loop_nested_no_writes(Stmt **body,size_t n,const char *name){
 for(size_t i=0;i<n;i++){
  Stmt *s=body[i];if(s->kind==S_VARIABLE&&!strcmp(s->name,name))return false;
  if(expr_writes_name(s->value,name)||expr_writes_name(s->condition,name))return false;
  if(!loop_nested_no_writes(s->body,s->body_count,name)||!loop_nested_no_writes(s->alternate,s->alt_count,name))return false;
 }
 return true;
}
static inline bool loop_body_safe_for_unroll(Stmt **body,size_t n,const char *name,int *increments){
 for(size_t i=0;i<n;i++){
  Stmt *s=body[i];
  if(s->kind==S_WHILE||s->kind==S_BREAK||s->kind==S_CONTINUE)return false;
  if(s->kind==S_VARIABLE&&!strcmp(s->name,name))return false;
  if(s->kind==S_EXPR&&s->value&&s->value->kind==E_BINARY&&s->value->token.kind==T_ASSIGN&&s->value->left&&s->value->left->kind==E_NAME&&!strcmp(s->value->left->token.text,name)){
   Expr *e=s->value->right;
   if(e&&e->kind==E_BINARY&&e->token.kind==T_PLUS&&e->left&&e->left->kind==E_NAME&&!strcmp(e->left->token.text,name)&&e->right&&e->right->kind==E_INT&&!strcmp(e->right->token.text,"1")){(*increments)++;continue;}
   return false;
  }
  if(expr_writes_name(s->value,name)||expr_writes_name(s->condition,name))return false;
  if(s->kind==S_IF&&(!loop_nested_no_writes(s->body,s->body_count,name)||!loop_nested_no_writes(s->alternate,s->alt_count,name)))return false;
 }
 return true;
}
static inline bool loop_unroll4_candidate(Stmt *s,Locals *l,long long *threshold,Local **counter){
 if(!s||s->kind!=S_WHILE||!s->condition||s->condition->kind!=E_BINARY)return false;
 Expr *cond=s->condition;
 if((cond->token.kind!=T_LT&&cond->token.kind!=T_LE)||!cond->left||cond->left->kind!=E_NAME||!cond->right||cond->right->kind!=E_INT)return false;
 char *end=NULL;long long bound=strtoll(cond->right->token.text,&end,10);
 if(!end||*end||bound<INT32_MIN+3LL||bound>INT32_MAX)return false;
 Local *v=local_find(l,cond->left->token.text);if(!v||v->reg<0)return false;
 int increments=0;if(!loop_body_safe_for_unroll(s->body,s->body_count,cond->left->token.text,&increments)||increments!=1)return false;
 *threshold=cond->token.kind==T_LT?bound-3:bound-2;
 *counter=v;return *threshold>=INT32_MIN&&*threshold<=INT32_MAX;
}
static inline void emit_block(Text *o,Stmt **body,size_t n,Locals *l,int *label,SwanError *err){
 for(size_t i=0;i<n;i++){
  Stmt *s=body[i];
  switch(s->kind){
   case S_VARIABLE:{Local *v=local_find(l,s->name);if(s->value)emit_expr(o,s->value,l,err);else text_add(o,"    xor eax, eax\n");if(v){if(v->reg>=0)text_add(o,"    mov %s, rax\n",cg_saved_regs[v->reg]);else text_add(o,"    mov QWORD PTR [rbp - %d], rax\n",v->slot*8);}break;}
   case S_LOG:
    if(s->value&&s->value->kind==E_STRING){
     int id=(*label)++;text_add(o,".section .rodata\n.Lswan_str_%d:\n    .string \"",id);
     for(const unsigned char *q=(const unsigned char*)s->value->token.text;*q;q++){
      if(*q==34||*q==92)text_add(o,"\\%c",*q);
      else if(*q==10)text_add(o,"\\n");
      else if(*q==9)text_add(o,"\\t");
      else if(*q==13)text_add(o,"\\r");
      else if(*q<32)text_add(o,"\\%03o",*q);
      else text_add(o,"%c",*q);
     }
     text_add(o,"\"\n.text\n    lea rdi, [rip + .Lswan_str_%d]\n    call puts@PLT\n",id);
    }else{emit_expr(o,s->value,l,err);text_add(o,"    mov rsi, rax\n    lea rdi, [rip + .Lswan_fmt]\n    xor eax, eax\n");cg_call(o,"printf@PLT");}
    break;
   case S_RETURN:if(s->value)emit_expr(o,s->value,l,err);else text_add(o,"    xor eax, eax\n");emit_restore_regs(o,l);text_add(o,"    leave\n    ret\n");break;
   case S_EXPR:if(!emit_accumulate_statement(o,s->value,l,err)&&!emit_increment_statement(o,s->value,l))emit_expr(o,s->value,l,err);break;
   case S_PASS:break;
   case S_BREAK:case S_CONTINUE:swan_error(err,&s->token,"loop control lowering is not yet supported");break;
   case S_IF:{
    int id=(*label)++;char target[64];snprintf(target,sizeof(target),".Lswan_else_%d",id);emit_condition_jump(o,s->condition,l,err,target,false);
    emit_block(o,s->body,s->body_count,l,label,err);text_add(o,"    jmp .Lswan_end_%d\n.Lswan_else_%d:\n",id,id);
    emit_block(o,s->alternate,s->alt_count,l,label,err);text_add(o,".Lswan_end_%d:\n",id);break;
   }
   case S_WHILE:{
    int id=(*label)++;long long threshold=0;Local *counter=NULL;
    if(loop_unroll4_candidate(s,l,&threshold,&counter)){
     text_add(o,".Lswan_loop4_%d:\n    cmp %s, %lld\n    jge .Lswan_tail_%d\n",id,cg_saved_regs[counter->reg],threshold,id);
     for(int u=0;u<4;u++)emit_block(o,s->body,s->body_count,l,label,err);
     text_add(o,"    jmp .Lswan_loop4_%d\n.Lswan_tail_%d:\n",id,id);
    }
    text_add(o,".Lswan_loop_%d:\n",id);char target[64];snprintf(target,sizeof(target),".Lswan_loop_end_%d",id);emit_condition_jump(o,s->condition,l,err,target,false);emit_block(o,s->body,s->body_count,l,label,err);
    text_add(o,"    jmp .Lswan_loop_%d\n.Lswan_loop_end_%d:\n",id,id);break;
   }
   default:break;
  }
 }
}
static inline bool swan_codegen_x86_64(Program *p,Text *o,SwanError *err){
 text_add(o,".intel_syntax noprefix\n.text\n");int strid=0;
 for(size_t i=0;i<p->count;i++){
  Stmt *fn=p->functions[i];Locals l={0};o->stack_depth=0;
  for(size_t a=0;a<fn->param_count;a++)local_add(&l,fn->params[a],false);
  collect_locals(fn->body,fn->body_count,&l);int frame=((l.slots+l.reg_count)*8+15)/16*16;
  char sym[SWAN_MAX_TEXT+16];if(!strcmp(fn->name,"main"))strcpy(sym,"main");else snprintf(sym,sizeof(sym),"swan_fn_%s",fn->name);
  text_add(o,".globl %s\n%s:\n    push rbp\n    mov rbp, rsp\n",sym,sym);if(frame)text_add(o,"    sub rsp, %d\n",frame);
  for(int a=0;a<l.reg_count;a++)text_add(o,"    mov QWORD PTR [rbp - %d], %s\n",l.slots*8+(a+1)*8,cg_saved_regs[a]);
  const char *regs[]={"rdi","rsi","rdx","rcx","r8","r9"};
  for(size_t a=0;a<fn->param_count&&a<6;a++){Local *v=local_find(&l,fn->params[a]);if(v){if(v->reg>=0)text_add(o,"    mov %s, %s\n",cg_saved_regs[v->reg],regs[a]);else text_add(o,"    mov QWORD PTR [rbp - %d], %s\n",v->slot*8,regs[a]);}}
  emit_block(o,fn->body,fn->body_count,&l,&strid,err);text_add(o,"    xor eax, eax\n");emit_restore_regs(o,&l);text_add(o,"    leave\n    ret\n");
 }
 text_add(o,".section .rodata\n.Lswan_fmt:\n    .string \"%%ld\\n\"\n.section .note.GNU-stack,\"\",@progbits\n");
 return err->message[0]==0;
}
#endif
