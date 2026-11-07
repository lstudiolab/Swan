#ifndef SWAN_CODEGEN_X86_64_H
#define SWAN_CODEGEN_X86_64_H
#include "swan/parser.h"
typedef struct { char *data; size_t len,cap; } Text;
static inline void text_add(Text *t,const char *fmt,...){va_list ap;va_start(ap,fmt);va_list cp;va_copy(cp,ap);int n=vsnprintf(NULL,0,fmt,cp);va_end(cp);if(n<0){va_end(ap);return;}size_t need=t->len+(size_t)n+1;if(need>t->cap){size_t cap=t->cap?t->cap:4096;while(cap<need)cap*=2;char *p=realloc(t->data,cap);if(!p){va_end(ap);return;}t->data=p;t->cap=cap;}vsnprintf(t->data+t->len,t->cap-t->len,fmt,ap);t->len+=(size_t)n;va_end(ap);}
typedef struct { char name[SWAN_MAX_TEXT]; int slot; bool mut; } Local;
typedef struct { Local items[1024];size_t count;int slots; } Locals;
static inline int local_find(Locals *l,const char *name){for(size_t i=l->count;i>0;i--)if(!strcmp(l->items[i-1].name,name))return l->items[i-1].slot;return -1;}
static inline void collect_locals(Stmt **body,size_t n,Locals *l){for(size_t i=0;i<n;i++){Stmt *s=body[i];if(s->kind==S_VARIABLE&&l->count<1024){strcpy(l->items[l->count].name,s->name);l->items[l->count].slot=++l->slots;l->items[l->count].mut=s->mutable_value;l->count++;}collect_locals(s->body,s->body_count,l);collect_locals(s->alternate,s->alt_count,l);}}
static inline void emit_expr(Text *o,Expr *e,Locals *l,SwanError *err);
static inline void emit_call(Text *o,Expr *e,Locals *l,SwanError *err){const char *name=e->left->token.text;if(!strcmp(name,"array")){swan_error(err,&e->token,"array lowering is not ported to the C backend yet");return;}if(e->argc>6){swan_error(err,&e->token,"x86-64 backend supports at most six arguments");return;}int area=(int)(((e->argc*8+15)/16)*16);if(area)text_add(o,"    sub rsp, %d\n",area);for(size_t i=0;i<e->argc;i++){emit_expr(o,e->args[i],l,err);text_add(o,"    mov QWORD PTR [rsp + %zu], rax\n",i*8);}const char *regs[]={"rdi","rsi","rdx","rcx","r8","r9"};for(size_t i=0;i<e->argc;i++)text_add(o,"    mov %s, QWORD PTR [rsp + %zu]\n",regs[i],i*8);text_add(o,"    xor eax, eax\n    call swan_fn_%s\n",name);if(area)text_add(o,"    add rsp, %d\n",area);}
static inline void emit_expr(Text *o,Expr *e,Locals *l,SwanError *err){if(!e)return;switch(e->kind){
 case E_INT:text_add(o,"    mov rax, %s\n",e->token.text);break;
 case E_BOOL:text_add(o,"    mov eax, %d\n",e->token.kind==T_TRUE);break;
 case E_FLOAT:swan_error(err,&e->token,"floating point is not yet supported by the C backend");break;
 case E_STRING:swan_error(err,&e->token,"string literal emission is not yet supported by the C backend");break;
 case E_NAME:{int slot=local_find(l,e->token.text);if(slot<0)swan_error(err,&e->token,"unknown variable '%s'",e->token.text);else text_add(o,"    mov rax, QWORD PTR [rbp - %d]\n",slot*8);break;}
 case E_UNARY:emit_expr(o,e->left,l,err);if(e->token.kind==T_MINUS)text_add(o,"    neg rax\n");else text_add(o,"    test rax, rax\n    sete al\n    movzx rax, al\n");break;
 case E_CALL:emit_call(o,e,l,err);break;
 case E_BINARY:
  if(e->token.kind==T_ASSIGN){
   if(e->left->kind!=E_NAME){swan_error(err,&e->token,"assignment target must be a variable");break;}
   int slot=local_find(l,e->left->token.text);
   if(slot<0){swan_error(err,&e->token,"unknown assignment target");break;}
   emit_expr(o,e->right,l,err);
   text_add(o,"    mov QWORD PTR [rbp - %d], rax\n",slot*8);
   break;
  }
  emit_expr(o,e->left,l,err);
  text_add(o,"    push rax\n");
  emit_expr(o,e->right,l,err);
  text_add(o,"    mov rcx, rax\n    pop rax\n");
  switch(e->token.kind){
   case T_PLUS:text_add(o,"    add rax, rcx\n");break;
   case T_MINUS:text_add(o,"    sub rax, rcx\n");break;
   case T_STAR:text_add(o,"    imul rax, rcx\n");break;
   case T_SLASH:case T_FLOORDIV:case T_PERCENT:
    text_add(o,"    cqo\n    idiv rcx\n");
    if(e->token.kind==T_PERCENT)text_add(o,"    mov rax, rdx\n");
    break;
   case T_EQ:case T_NE:case T_LT:case T_GT:case T_LE:case T_GE:{
    const char *cc=e->token.kind==T_EQ?"sete":e->token.kind==T_NE?"setne":e->token.kind==T_LT?"setl":e->token.kind==T_GT?"setg":e->token.kind==T_LE?"setle":"setge";
    text_add(o,"    cmp rax, rcx\n    %s al\n    movzx rax, al\n",cc);
    break;
   }
   default:swan_error(err,&e->token,"operator not supported by backend");break;
  }
  break;
}
}
static inline void emit_block(Text *o,Stmt **body,size_t n,Locals *l,int *label,SwanError *err){for(size_t i=0;i<n;i++){Stmt *s=body[i];switch(s->kind){
 case S_VARIABLE:{int slot=local_find(l,s->name);if(s->value)emit_expr(o,s->value,l,err);else text_add(o,"    xor eax, eax\n");if(slot>0)text_add(o,"    mov QWORD PTR [rbp - %d], rax\n",slot*8);break;}
 case S_LOG:if(s->value&&s->value->kind==E_STRING){int id=(*label)++;text_add(o,".section .rodata\n.Lswan_str_%d:\n    .string \"",id);for(const unsigned char *q=(const unsigned char*)s->value->token.text;*q;q++){if(*q=='"'||*q=='\\')text_add(o,"\\%c",*q);else if(*q=='\n')text_add(o,"\\n");else if(*q=='\t')text_add(o,"\\t");else if(*q=='\r')text_add(o,"\\r");else if(*q<32)text_add(o,"\\%03o",*q);else text_add(o,"%c",*q);}text_add(o,"\"\n.text\n    lea rdi, [rip + .Lswan_str_%d]\n    call puts@PLT\n",id);}else{emit_expr(o,s->value,l,err);text_add(o,"    mov rsi, rax\n    lea rdi, [rip + .Lswan_fmt]\n    xor eax, eax\n    call printf@PLT\n");}break;
 case S_RETURN:if(s->value)emit_expr(o,s->value,l,err);else text_add(o,"    xor eax, eax\n");text_add(o,"    leave\n    ret\n");break;
 case S_EXPR:emit_expr(o,s->value,l,err);break;
 case S_PASS:break;
 case S_BREAK:case S_CONTINUE:swan_error(err,&s->token,"loop control lowering is not yet supported");break;
 case S_IF:{int id=(*label)++;emit_expr(o,s->condition,l,err);text_add(o,"    test rax, rax\\n    jz .Lswan_else_%d\\n",id);emit_block(o,s->body,s->body_count,l,label,err);text_add(o,"    jmp .Lswan_end_%d\\n.Lswan_else_%d:\\n",id,id);emit_block(o,s->alternate,s->alt_count,l,label,err);text_add(o,".Lswan_end_%d:\\n",id);break;}
 case S_WHILE:{int id=(*label)++;text_add(o,".Lswan_loop_%d:\\n",id);emit_expr(o,s->condition,l,err);text_add(o,"    test rax, rax\\n    jz .Lswan_loop_end_%d\\n",id);emit_block(o,s->body,s->body_count,l,label,err);text_add(o,"    jmp .Lswan_loop_%d\\n.Lswan_loop_end_%d:\\n",id,id);break;}
 default:break;
 }}}
static inline bool swan_codegen_x86_64(Program *p,Text *o,SwanError *err){
 text_add(o,".intel_syntax noprefix\n.text\n");
 int strid=0;
 for(size_t i=0;i<p->count;i++){
  Stmt *fn=p->functions[i];
  Locals l={0};
  for(size_t a=0;a<fn->param_count;a++){
   strcpy(l.items[l.count].name,fn->params[a]);
   l.items[l.count].slot=++l.slots;
   l.count++;
  }
  collect_locals(fn->body,fn->body_count,&l);
  int frame=((l.slots*8+15)/16)*16;
  char sym[SWAN_MAX_TEXT+16];
  if(!strcmp(fn->name,"main"))strcpy(sym,"main");
  else snprintf(sym,sizeof(sym),"swan_fn_%s",fn->name);
  text_add(o,".globl %s\n%s:\n    push rbp\n    mov rbp, rsp\n",sym,sym);
  if(frame)text_add(o,"    sub rsp, %d\n",frame);
  const char *regs[]={"rdi","rsi","rdx","rcx","r8","r9"};
  for(size_t a=0;a<fn->param_count&&a<6;a++){
   int slot=local_find(&l,fn->params[a]);
   text_add(o,"    mov QWORD PTR [rbp - %d], %s\n",slot*8,regs[a]);
  }
  emit_block(o,fn->body,fn->body_count,&l,&strid,err);
  text_add(o,"    xor eax, eax\n    leave\n    ret\n");
 }
 text_add(o,".section .rodata\n.Lswan_fmt:\n    .string \"%%ld\\n\"\n.section .note.GNU-stack,\"\",@progbits\n");
 return err->message[0]==0;
}
#endif
