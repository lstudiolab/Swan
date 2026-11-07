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
static inline void emit_expr(Text *o,Expr *e,Locals *l,SwanError *err);
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
     uint64_t magic=(~(uint64_t)0)/divisor+1;
     text_add(o,"    test rax, rax\n    js 1f\n    mov rcx, rax\n    movabs rdx, 0x%llx\n    mul rdx\n    imul rdx, rdx, %lld\n    sub rcx, rdx\n    jns 2f\n    add rcx, %lld\n    jmp 2f\n1:\n    mov rcx, %lld\n    cqo\n    idiv rcx\n    mov rcx, rdx\n2:\n    mov rax, rcx\n",
      (unsigned long long)magic,imm,imm,imm);break;
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
   case S_EXPR:emit_expr(o,s->value,l,err);break;
   case S_PASS:break;
   case S_BREAK:case S_CONTINUE:swan_error(err,&s->token,"loop control lowering is not yet supported");break;
   case S_IF:{
    int id=(*label)++;emit_expr(o,s->condition,l,err);text_add(o,"    test rax, rax\n    jz .Lswan_else_%d\n",id);
    emit_block(o,s->body,s->body_count,l,label,err);text_add(o,"    jmp .Lswan_end_%d\n.Lswan_else_%d:\n",id,id);
    emit_block(o,s->alternate,s->alt_count,l,label,err);text_add(o,".Lswan_end_%d:\n",id);break;
   }
   case S_WHILE:{
    int id=(*label)++;text_add(o,".Lswan_loop_%d:\n",id);emit_expr(o,s->condition,l,err);
    text_add(o,"    test rax, rax\n    jz .Lswan_loop_end_%d\n",id);emit_block(o,s->body,s->body_count,l,label,err);
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
