#ifndef SWAN_CODEGEN_X86_64_H
#define SWAN_CODEGEN_X86_64_H
#include "swan/parser.h"
typedef struct { char *data; size_t len,cap; int stack_depth; bool frameless,uses_array_bounds,failed; int next_label,loop_depth,loop_break[128],loop_continue[128]; Expr *hoist_expr,*recurrence_expr,*inline_arg,*active_loop_condition; const char *inline_param; Program *program; } Text;
static inline bool text_reserve(Text *t,size_t extra){
 if(extra>SIZE_MAX-t->len-1)return false;
 size_t need=t->len+extra+1;
 if(need<=t->cap)return true;
 size_t cap=t->cap?t->cap:4096;
 while(cap<need){if(cap>SIZE_MAX/2){cap=need;break;}cap*=2;}
 char *p=realloc(t->data,cap);if(!p)return false;t->data=p;t->cap=cap;if(!t->len)t->data[0]=0;return true;
}
static inline void text_add(Text *t,const char *fmt,...){
 if(!strchr(fmt,'%')){
  size_t n=strlen(fmt);if(!text_reserve(t,n)){t->failed=true;return;}
  memcpy(t->data+t->len,fmt,n);t->len+=n;t->data[t->len]=0;return;
 }
 va_list ap;va_start(ap,fmt);va_list retry;va_copy(retry,ap);
 if(!text_reserve(t,0)){t->failed=true;va_end(retry);va_end(ap);return;}
 size_t avail=t->cap-t->len;int n=vsnprintf(t->data+t->len,avail,fmt,ap);
 if(n<0){t->failed=true;va_end(retry);va_end(ap);return;}
 if((size_t)n>=avail){
  if(!text_reserve(t,(size_t)n)){t->failed=true;va_end(retry);va_end(ap);return;}
  vsnprintf(t->data+t->len,t->cap-t->len,fmt,retry);
 }
 t->len+=(size_t)n;va_end(retry);va_end(ap);
}
static inline void cg_reserve(Text *o,int n){if(n>0){text_add(o,"    sub rsp, %d\n",n);o->stack_depth+=n;}}
static inline void cg_release(Text *o,int n){if(n>0){text_add(o,"    add rsp, %d\n",n);o->stack_depth-=n;}}
static inline void cg_push(Text *o,const char *reg){text_add(o,"    push %s\n",reg);o->stack_depth+=8;}
static inline void cg_pop(Text *o,const char *reg){text_add(o,"    pop %s\n",reg);o->stack_depth-=8;}
static inline void cg_call(Text *o,const char *symbol){int entry=o->frameless?8:0;int pad=(16-((o->stack_depth+entry)%16))%16;cg_reserve(o,pad);text_add(o,"    call %s\n",symbol);cg_release(o,pad);}
typedef struct { const char *name; int slot,reg,next; bool mut,array_u32,array_unchecked; } Local;
typedef struct { Local items[1024];int buckets[2048];size_t count;int slots,reg_count,saved_reg_count,volatile_reg_count;bool caller_saved; } Locals;
static const char *cg_saved_regs[]={"rbx","r12","r13","r14","r15"};
static const char *cg_volatile_regs[]={"rdi","rsi","rdx","r8","r9"};
static const char *cg_volatile_low_regs[]={"edi","esi","edx","r8d","r9d"};
static inline const char *cg_reg_name(int reg){return reg<5?cg_saved_regs[reg]:cg_volatile_regs[reg-5];}
static inline const char *cg_reg_low_name(int reg){static const char *saved[]={"ebx","r12d","r13d","r14d","r15d"};return reg<5?saved[reg]:cg_volatile_low_regs[reg-5];}
static inline unsigned cg_name_hash(const char *s){uint32_t h=2166136261u;for(;*s;s++)h=(h^(unsigned char)*s)*16777619u;return h;}
static inline Local *local_find(Locals *l,const char *name){unsigned b=cg_name_hash(name)&2047u;for(int n=l->buckets[b];n;n=l->items[n-1].next)if(!strcmp(l->items[n-1].name,name))return &l->items[n-1];return NULL;}
static inline Local *local_add(Locals *l,const char *name,bool mut){
 if(l->count>=1024)return NULL;
 size_t i=l->count++;unsigned b=cg_name_hash(name)&2047u;Local *v=&l->items[i];v->name=name;v->mut=mut;v->reg=-1;v->slot=0;v->next=l->buckets[b];l->buckets[b]=(int)i+1;
 if(!l->caller_saved&&l->saved_reg_count<5){v->reg=l->saved_reg_count++;l->reg_count++;}
 else if(l->volatile_reg_count<5){v->reg=5+l->volatile_reg_count++;l->reg_count++;}
 else if(l->saved_reg_count<5){v->reg=l->saved_reg_count++;l->reg_count++;}
 else v->slot=++l->slots;
 return v;
}
static inline void collect_locals(Stmt **body,size_t n,Locals *l){for(size_t i=0;i<n;i++){Stmt *s=body[i];if(s->kind==S_VARIABLE){Local *v=local_add(l,s->name,s->mutable_value);if(v&&s->value&&s->value->kind==E_CALL&&s->value->left&&s->value->left->kind==E_NAME){
 if(!strcmp(s->value->left->token.text,"array")||!strcmp(s->value->left->token.text,"array_u32"))v->array_u32=true;
 if(!strcmp(s->value->left->token.text,"array_unchecked")||!strcmp(s->value->left->token.text,"array_u32_unchecked")){v->array_u32=true;v->array_unchecked=true;}
}}collect_locals(s->body,s->body_count,l);collect_locals(s->alternate,s->alt_count,l);}}
static inline void emit_restore_regs(Text *o,Locals *l){
 for(int i=0;i<l->saved_reg_count;i++)text_add(o,"    mov %s, QWORD PTR [rbp - %d]\n",cg_saved_regs[i],l->slots*8+(i+1)*8);
}
static inline void cg_save_volatile(Text *o,Locals *l){for(int i=0;i<l->volatile_reg_count;i++)cg_push(o,cg_volatile_regs[i]);}
static inline void cg_restore_volatile(Text *o,Locals *l){for(int i=l->volatile_reg_count-1;i>=0;i--)cg_pop(o,cg_volatile_regs[i]);}
static inline bool expr_has_call(Expr *e){return e&&e->contains_call;}
static inline unsigned cg_function_hash(const char *s){uint32_t h=2166136261u;for(;*s;s++)h=(h^(unsigned char)*s)*16777619u;return h;}
static inline Stmt *cg_find_function(Program *p,const char *name){
 if(!p)return NULL;
 if(p->function_indexed){unsigned b=cg_function_hash(name)&2047u;for(int n=p->function_buckets[b];n;n=p->function_next[n-1]){Stmt *fn=p->functions[n-1];if(!strcmp(fn->name,name))return fn;}return NULL;}
 for(size_t i=0;i<p->count;i++)if(!strcmp(p->functions[i]->name,name))return p->functions[i];
 return NULL;
}
static inline bool expr_is_nonnegative_midpoint(Expr *division,Expr *condition){
 if(!division||division->kind!=E_BINARY||(division->token.kind!=T_SLASH&&division->token.kind!=T_FLOORDIV)||!division->right||division->right->kind!=E_INT)return false;
 if(!condition||condition->kind!=E_BINARY||(condition->token.kind!=T_LE&&condition->token.kind!=T_LT)||!condition->left||!condition->right||condition->left->kind!=E_NAME||condition->right->kind!=E_NAME)return false;
 Expr *difference=division->left;
 return difference&&difference->kind==E_BINARY&&difference->token.kind==T_MINUS&&difference->left&&difference->left->kind==E_NAME&&difference->right&&difference->right->kind==E_NAME&&!strcmp(difference->left->token.text,condition->right->token.text)&&!strcmp(difference->right->token.text,condition->left->token.text);
}
static inline bool stmt_is_safe_midpoint(Stmt *statement,Expr *condition){
 if(!statement||statement->kind!=S_VARIABLE||!statement->value||statement->value->kind!=E_BINARY||statement->value->token.kind!=T_PLUS||!statement->value->left||statement->value->left->kind!=E_NAME||!condition||condition->kind!=E_BINARY||!condition->left||condition->left->kind!=E_NAME||strcmp(statement->value->left->token.text,condition->left->token.text))return false;
 return expr_is_nonnegative_midpoint(statement->value->right,condition);
}
static inline void emit_expr(Text *o,Expr *e,Locals *l,SwanError *err);
static inline void emit_condition_jump(Text *o,Expr *e,Locals *l,SwanError *err,const char *target,bool when_true);
static inline bool add_term_to_r10_supported(Expr *e,Locals *l){
 if(!e)return false;
 if(e->kind==E_INT){
  char *end=NULL;long long value=strtoll(e->token.text,&end,10);
  return end&&!*end&&value>=INT32_MIN&&value<=INT32_MAX;
 }
 if(e->kind==E_BOOL)return true;
 if(e->kind==E_NAME)return local_find(l,e->token.text)!=NULL;
 if(e->kind==E_BINARY&&e->token.kind==T_STAR&&e->left&&e->left->kind==E_NAME&&e->right&&e->right->kind==E_INT){
  Local *v=local_find(l,e->left->token.text);char *end=NULL;long long value=strtoll(e->right->token.text,&end,10);
  return v&&end&&!*end&&value>=INT32_MIN&&value<=INT32_MAX;
 }
 return false;
}
static inline bool add_chain_supported(Expr *e,Locals *l){
 if(e&&e->kind==E_BINARY&&e->token.kind==T_PLUS)return add_chain_supported(e->left,l)&&add_chain_supported(e->right,l);
 return add_term_to_r10_supported(e,l);
}
static inline void emit_term_r10(Text *o,Expr *e,Locals *l){
 if(o->hoist_expr==e){text_add(o,"    mov r10, r11\n");return;}
 if(o->recurrence_expr==e){text_add(o,"    mov r10, r15\n");return;}
 if(e->kind==E_INT)text_add(o,"    mov r10, %s\n",e->token.text);
 else if(e->kind==E_BOOL)text_add(o,"    mov r10, %d\n",e->token.kind==T_TRUE);
 else if(e->kind==E_NAME){
  Local *v=local_find(l,e->token.text);
  if(v->reg>=0)text_add(o,"    mov r10, %s\n",cg_reg_name(v->reg));
  else text_add(o,"    mov r10, QWORD PTR [rbp - %d]\n",v->slot*8);
 }else if(e->kind==E_BINARY&&e->token.kind==T_STAR){
  Local *v=local_find(l,e->left->token.text);
  if(v->reg>=0)text_add(o,"    imul r10, %s, %s\n",cg_reg_name(v->reg),e->right->token.text);
  else text_add(o,"    imul r10, QWORD PTR [rbp - %d], %s\n",v->slot*8,e->right->token.text);
 }
}
static inline void emit_add_terms_rax(Text *o,Expr *e,Locals *l,SwanError *err,bool *first){
 if(o->hoist_expr==e){
  if(*first){text_add(o,"    mov rax, r11\n");*first=false;}
  else text_add(o,"    mov r10, r11\n    add rax, r10\n");
  return;
 }
 if(o->recurrence_expr==e){
  if(*first){text_add(o,"    mov rax, r15\n");*first=false;}
  else text_add(o,"    add rax, r15\n");
  return;
 }
 if(e&&e->kind==E_BINARY&&e->token.kind==T_PLUS){
  emit_add_terms_rax(o,e->left,l,err,first);emit_add_terms_rax(o,e->right,l,err,first);return;
 }
 if(*first){emit_expr(o,e,l,err);*first=false;}
 else{emit_term_r10(o,e,l);text_add(o,"    add rax, r10\n");}
}
static inline void emit_add_terms(Text *o,Expr *e,Locals *l,SwanError *err,bool *first){
 if(o->hoist_expr==e){
  if(*first){text_add(o,"    mov r10, r11\n");*first=false;}
  else text_add(o,"    add r10, r11\n");
  return;
 }
 if(o->recurrence_expr==e){
  if(*first){text_add(o,"    mov r10, r15\n");*first=false;}
  else text_add(o,"    add r10, r15\n");
  return;
 }
 if(e&&e->kind==E_BINARY&&e->token.kind==T_PLUS){
  emit_add_terms(o,e->left,l,err,first);emit_add_terms(o,e->right,l,err,first);return;
 }
 emit_expr(o,e,l,err);
 if(*first){text_add(o,"    mov r10, rax\n");*first=false;}
 else text_add(o,"    add r10, rax\n");
}
static inline bool expr_writes_local(Expr *e,const char *name);
static inline bool emit_accumulate_statement(Text *o,Expr *e,Locals *l,SwanError *err){
 if(!e||e->kind!=E_BINARY||e->token.kind!=T_ASSIGN||!e->left||e->left->kind!=E_NAME)return false;
 Expr *rhs=e->right;
 if(!rhs||rhs->kind!=E_BINARY||
    (rhs->token.kind!=T_PLUS&&rhs->token.kind!=T_MINUS&&rhs->token.kind!=T_STAR)||
    !rhs->left||rhs->left->kind!=E_NAME||
    strcmp(rhs->left->token.text,e->left->token.text)||
    !rhs->right||expr_writes_local(rhs->right,e->left->token.text))return false;
 Local *dst=local_find(l,e->left->token.text);if(!dst)return false;
 const char *op=rhs->token.kind==T_PLUS?"add":rhs->token.kind==T_MINUS?"sub":"imul";
 if(rhs->right->kind==E_INT){
  char *end=NULL;long long imm=strtoll(rhs->right->token.text,&end,10);
  if(end&&!*end&&imm>=INT32_MIN&&imm<=INT32_MAX){
   if(rhs->token.kind==T_STAR){
    uint64_t magnitude=imm<0?(uint64_t)(-(imm+1))+1:(uint64_t)imm;
    bool power_of_two=magnitude&&!(magnitude&(magnitude-1));
    unsigned shift=0;while(power_of_two&&(UINT64_C(1)<<shift)!=magnitude&&shift<63)shift++;
    if(imm==0){
     if(dst->reg>=0)text_add(o,"    xor %s, %s\n",cg_reg_low_name(dst->reg),cg_reg_low_name(dst->reg));
     else text_add(o,"    mov QWORD PTR [rbp - %d], 0\n",dst->slot*8);
    }else if(imm==1){}
    else if(power_of_two){
     const char *target=dst->reg>=0?cg_reg_name(dst->reg):"rax";
     if(dst->reg<0)text_add(o,"    mov rax, QWORD PTR [rbp - %d]\n",dst->slot*8);
     text_add(o,"    shl %s, %u\n",target,shift);
     if(imm<0)text_add(o,"    neg %s\n",target);
     if(dst->reg<0)text_add(o,"    mov QWORD PTR [rbp - %d], rax\n",dst->slot*8);
    }else if(dst->reg>=0)text_add(o,"    imul %s, %s, %lld\n",cg_reg_name(dst->reg),cg_reg_name(dst->reg),imm);
    else text_add(o,"    mov rax, QWORD PTR [rbp - %d]\n    imul rax, rax, %lld\n    mov QWORD PTR [rbp - %d], rax\n",dst->slot*8,imm,dst->slot*8);
   }else if(dst->reg>=0)text_add(o,"    %s %s, %lld\n",op,cg_reg_name(dst->reg),imm);
   else text_add(o,"    %s QWORD PTR [rbp - %d], %lld\n",op,dst->slot*8,imm);
   return true;
  }
 }
 if(rhs->right->kind==E_NAME){
  Local *src=local_find(l,rhs->right->token.text);
  if(src){
   const char *operand=src->reg>=0?cg_reg_name(src->reg):NULL;
   char memory[64];if(!operand){snprintf(memory,sizeof(memory),"QWORD PTR [rbp - %d]",src->slot*8);operand=memory;}
   if(dst->reg>=0)text_add(o,"    %s %s, %s\n",op,cg_reg_name(dst->reg),operand);
   else text_add(o,"    mov rax, QWORD PTR [rbp - %d]\n    %s rax, %s\n    mov QWORD PTR [rbp - %d], rax\n",dst->slot*8,op,operand,dst->slot*8);
   return true;
  }
 }
 emit_expr(o,rhs->right,l,err);
 if(dst->reg>=0)text_add(o,"    %s %s, rax\n",op,cg_reg_name(dst->reg));
 else text_add(o,"    %s QWORD PTR [rbp - %d], rax\n",op,dst->slot*8);
 return true;
}
static inline bool expr_inline_pure(Expr *e,const char *parameter,bool only_parameter){
 if(!e)return false;
 if(e->kind==E_INT||e->kind==E_BOOL)return true;
 if(e->kind==E_NAME)return !only_parameter||!strcmp(e->token.text,parameter);
 if(e->kind==E_UNARY)return (e->token.kind==T_MINUS||e->token.kind==T_NOT)&&expr_inline_pure(e->left,parameter,only_parameter);
 if(e->kind==E_BINARY){
  if(e->token.kind==T_ASSIGN||e->token.kind==T_AND||e->token.kind==T_OR)return false;
  return expr_inline_pure(e->left,parameter,only_parameter)&&expr_inline_pure(e->right,parameter,only_parameter);
 }
 return false;
}
static inline void emit_call(Text *o,Expr *e,Locals *l,SwanError *err){
 const char *name=e->left->token.text;
 if(!strcmp(name,"array")||!strcmp(name,"array_u32")||!strcmp(name,"array_unchecked")||!strcmp(name,"array_u32_unchecked")){
  if(e->argc!=1){swan_error(err,&e->token,"array expects exactly one length argument");return;}
  cg_save_volatile(o,l);emit_expr(o,e->args[0],l,err);
  if(!strcmp(name,"array")||!strcmp(name,"array_u32")){
   text_add(o,"    test rax, rax\n    js .Lswan_array_bounds_fail\n");
   cg_push(o,"rax");
   text_add(o,"    add rax, 2\n    jo .Lswan_array_bounds_fail\n    mov rdi, rax\n    mov esi, 4\n");
   cg_call(o,"calloc@PLT");
   cg_pop(o,"rcx");
   text_add(o,"    test rax, rax\n    jz .Lswan_array_bounds_fail\n    mov QWORD PTR [rax], rcx\n    add rax, 8\n");
  }else{
   int id=o->next_label++;text_add(o,"    test rax, rax\n    js .Lswan_array_bounds_fail\n    mov rdi, rax\n    test rdi, rdi\n    jnz .Lswan_array_nonzero_%d\n    mov edi, 1\n.Lswan_array_nonzero_%d:\n    mov esi, 4\n",id,id);
   cg_call(o,"calloc@PLT");
   text_add(o,"    test rax, rax\n    jz .Lswan_array_bounds_fail\n");
  }
  cg_restore_volatile(o,l);o->uses_array_bounds=true;
  return;
 }
 if(e->argc>6){swan_error(err,&e->token,"x86-64 backend supports at most six arguments");return;}
 if(o->program&&e->argc==1&&expr_inline_pure(e->args[0],NULL,false)){
  Stmt *fn=cg_find_function(o->program,name);
  if(fn&&fn->param_count==1&&fn->body_count==1&&fn->body[0]->kind==S_RETURN&&fn->body[0]->value){
   const char *parameter=fn->params[0];Expr *body=fn->body[0]->value;
   if(expr_inline_pure(body,parameter,true)){
    Expr *saved_arg=o->inline_arg;const char *saved_param=o->inline_param;
    o->inline_arg=e->args[0];o->inline_param=parameter;
    emit_expr(o,body,l,err);
    o->inline_arg=saved_arg;o->inline_param=saved_param;
    return;
   }
  }
 }
 const char *regs[]={"rdi","rsi","rdx","rcx","r8","r9"};
 char symbol[SWAN_MAX_TEXT+16];if(!strcmp(name,"main"))strcpy(symbol,"main");else snprintf(symbol,sizeof(symbol),"swan_fn_%s",name);
 bool nested=false;for(size_t i=0;i<e->argc;i++)if(expr_has_call(e->args[i]))nested=true;
 cg_save_volatile(o,l);
 if(!nested){
  int entry=o->frameless?8:0;int pad=(16-((o->stack_depth+entry)%16))%16;cg_reserve(o,pad);
  for(size_t i=0;i<e->argc;i++){emit_expr(o,e->args[i],l,err);text_add(o,"    mov %s, rax\n",regs[i]);}
  text_add(o,"    call %s\n",symbol);cg_release(o,pad);cg_restore_volatile(o,l);return;
 }
 int area=(int)(((e->argc*8+15)/16)*16);
 int entry=o->frameless?8:0;int pad=(16-((o->stack_depth+entry+area)%16))%16;
 cg_reserve(o,area+pad);
 for(size_t i=0;i<e->argc;i++){emit_expr(o,e->args[i],l,err);text_add(o,"    mov QWORD PTR [rsp + %d], rax\n",pad+(int)i*8);}
 for(size_t i=0;i<e->argc;i++)text_add(o,"    mov %s, QWORD PTR [rsp + %d]\n",regs[i],pad+(int)i*8);
 text_add(o,"    call %s\n",symbol);cg_release(o,area+pad);cg_restore_volatile(o,l);
}
static inline bool cg_const_value(Expr *e,int64_t *out);
static inline bool cg_const_value_uncached(Expr *e,int64_t *out){
 if(!e)return false;
 if(e->kind==E_INT){char *end=NULL;errno=0;long long v=strtoll(e->token.text,&end,10);if(errno==ERANGE||!end||*end)return false;*out=(int64_t)v;return true;}
 if(e->kind==E_BOOL){*out=e->token.kind==T_TRUE;return true;}
 if(e->kind==E_UNARY&&e->token.kind==T_MINUS){int64_t v;if(!cg_const_value(e->left,&v))return false;*out=(int64_t)(0-(uint64_t)v);return true;}
 if(e->kind!=E_BINARY||e->token.kind==T_ASSIGN)return false;
 int64_t a,b;if(!cg_const_value(e->left,&a))return false;
 if(e->token.kind==T_AND&&!a){*out=0;return true;}
 if(e->token.kind==T_OR&&a){*out=1;return true;}
 if(!cg_const_value(e->right,&b))return false;
 switch(e->token.kind){
  case T_PLUS:*out=(int64_t)((uint64_t)a+(uint64_t)b);return true;
  case T_MINUS:*out=(int64_t)((uint64_t)a-(uint64_t)b);return true;
  case T_STAR:*out=(int64_t)((uint64_t)a*(uint64_t)b);return true;
  case T_EQ:*out=a==b;return true;case T_NE:*out=a!=b;return true;
  case T_AND:*out=(a!=0)&&(b!=0);return true;case T_OR:*out=(a!=0)||(b!=0);return true;
  case T_LT:*out=a<b;return true;case T_GT:*out=a>b;return true;
  case T_LE:*out=a<=b;return true;case T_GE:*out=a>=b;return true;
  case T_SLASH:case T_FLOORDIV:case T_PERCENT:
   if(!b||(a==INT64_MIN&&b==-1))return false;
   *out=e->token.kind==T_PERCENT?a%b:a/b;return true;
  default:return false;
 }
}
static inline bool cg_const_value(Expr *e,int64_t *out){
 if(!e)return false;
 if(e->constant_checked){if(e->constant_valid)*out=e->constant_value;return e->constant_valid;}
 int64_t value=0;bool valid=cg_const_value_uncached(e,&value);
 e->constant_checked=true;e->constant_valid=valid;
 if(valid){e->constant_value=value;*out=value;}
 return valid;
}
static inline void emit_expr(Text *o,Expr *e,Locals *l,SwanError *err){
 if(!e)return;
 if(o->hoist_expr==e){text_add(o,"    mov rax, r11\n");return;}
 if(o->recurrence_expr==e){text_add(o,"    mov rax, r15\n");return;}
 int64_t constant;
 if(cg_const_value(e,&constant)){
  if(constant>=INT32_MIN&&constant<=INT32_MAX)text_add(o,"    mov rax, %lld\n",(long long)constant);
  else text_add(o,"    movabs rax, 0x%016llx\n",(unsigned long long)(uint64_t)constant);
  return;
 }
 switch(e->kind){
 case E_INT:text_add(o,"    mov rax, %s\n",e->token.text);break;
 case E_BOOL:text_add(o,"    mov eax, %d\n",e->token.kind==T_TRUE);break;
 case E_FLOAT:swan_error(err,&e->token,"floating point is not yet supported by the C backend");break;
 case E_STRING:swan_error(err,&e->token,"string values are not yet supported outside log statements");break;
 case E_NAME:{if(o->inline_param&&!strcmp(e->token.text,o->inline_param)){Expr *arg=o->inline_arg;const char *param=o->inline_param;o->inline_arg=NULL;o->inline_param=NULL;emit_expr(o,arg,l,err);o->inline_arg=arg;o->inline_param=param;break;}Local *v=local_find(l,e->token.text);if(!v)swan_error(err,&e->token,"unknown variable '%s'",e->token.text);else if(v->reg>=0)text_add(o,"    mov rax, %s\n",cg_reg_name(v->reg));else text_add(o,"    mov rax, QWORD PTR [rbp - %d]\n",v->slot*8);break;}
 case E_UNARY:emit_expr(o,e->left,l,err);if(e->token.kind==T_MINUS)text_add(o,"    neg rax\n");else text_add(o,"    test rax, rax\n    sete al\n    movzx rax, al\n");break;
 case E_CALL:emit_call(o,e,l,err);break;
 case E_INDEX:{
  if(!e->left||e->left->kind!=E_NAME){swan_error(err,&e->token,"array base must be a local array variable");break;}
  Local *array=local_find(l,e->left->token.text);
  if(!array||!array->array_u32){swan_error(err,&e->token,"indexing currently requires an array_u32 value");break;}
  Expr *index=e->right;Local *index_local=index&&index->kind==E_NAME?local_find(l,index->token.text):NULL;int64_t index_constant=0;
  if(index_local){if(index_local->reg>=0)text_add(o,"    mov rcx, %s\n",cg_reg_name(index_local->reg));else text_add(o,"    mov rcx, QWORD PTR [rbp - %d]\n",index_local->slot*8);}
  else if(cg_const_value(index,&index_constant)){if(index_constant>=INT32_MIN&&index_constant<=INT32_MAX)text_add(o,"    mov ecx, %lld\n",(long long)index_constant);else text_add(o,"    movabs rcx, 0x%016llx\n",(unsigned long long)(uint64_t)index_constant);}
  else{emit_expr(o,index,l,err);text_add(o,"    mov rcx, rax\n");}
  if(array->reg>=0){const char *base=cg_reg_name(array->reg);if(!array->array_unchecked)text_add(o,"    cmp rcx, QWORD PTR [%s - 8]\n    jae .Lswan_array_bounds_fail\n",base);text_add(o,"    mov eax, DWORD PTR [%s + rcx*4]\n",base);}
  else{text_add(o,"    mov r10, QWORD PTR [rbp - %d]\n",array->slot*8);if(!array->array_unchecked)text_add(o,"    cmp rcx, QWORD PTR [r10 - 8]\n    jae .Lswan_array_bounds_fail\n");text_add(o,"    mov eax, DWORD PTR [r10 + rcx*4]\n");}
  o->uses_array_bounds=true;
  break;
 }
 case E_BINARY:
  if(e->token.kind==T_AND||e->token.kind==T_OR){int id=o->next_label++;char target[64];snprintf(target,sizeof(target),".Lswan_logic_true_%d",id);emit_condition_jump(o,e,l,err,target,true);text_add(o,"    xor eax, eax\n    jmp .Lswan_logic_end_%d\n%s:\n    mov eax, 1\n.Lswan_logic_end_%d:\n",id,target,id);break;}
  if(e->token.kind==T_PLUS&&!expr_has_call(e)){
   bool first=true;
   if(add_chain_supported(e,l))emit_add_terms_rax(o,e,l,err,&first);
   else{emit_add_terms(o,e,l,err,&first);if(!first)text_add(o,"    mov rax, r10\n");}
   break;
  }
  if(e->token.kind==T_ASSIGN){
   if(e->left&&e->left->kind==E_INDEX){
    Expr *base=e->left->left;
    if(!base||base->kind!=E_NAME){swan_error(err,&e->token,"array base must be a local variable");break;}
    Local *array=local_find(l,base->token.text);
    if(!array||!array->array_u32){swan_error(err,&e->token,"indexed assignment requires an array_u32 value");break;}
    emit_expr(o,e->left->right,l,err);cg_push(o,"rax");emit_expr(o,e->right,l,err);cg_pop(o,"rcx");
    if(array->reg>=0){const char *base=cg_reg_name(array->reg);if(!array->array_unchecked)text_add(o,"    cmp rcx, QWORD PTR [%s - 8]\n    jae .Lswan_array_bounds_fail\n",base);text_add(o,"    mov DWORD PTR [%s + rcx*4], eax\n",base);}
    else{text_add(o,"    mov r10, QWORD PTR [rbp - %d]\n",array->slot*8);if(!array->array_unchecked)text_add(o,"    cmp rcx, QWORD PTR [r10 - 8]\n    jae .Lswan_array_bounds_fail\n");text_add(o,"    mov DWORD PTR [r10 + rcx*4], eax\n");}
    o->uses_array_bounds=true;break;
   }
   if(e->left->kind!=E_NAME){swan_error(err,&e->token,"assignment target must be a variable or array element");break;}
   Local *v=local_find(l,e->left->token.text);
   if(!v){swan_error(err,&e->token,"unknown assignment target");break;}
   emit_expr(o,e->right,l,err);
   if(v->reg>=0)text_add(o,"    mov %s, rax\n",cg_reg_name(v->reg));
   else text_add(o,"    mov QWORD PTR [rbp - %d], rax\n",v->slot*8);
   break;
  }
  int64_t right_constant=0;
  if(e->right&&cg_const_value(e->right,&right_constant)){
   long long imm=(long long)right_constant;
   if(imm>=INT32_MIN&&imm<=INT32_MAX){
    if(e->left&&e->left->kind==E_NAME){
     Local *v=local_find(l,e->left->token.text);
     if(v&&v->reg>=0){
      const char *reg=cg_reg_name(v->reg);
      if(e->token.kind==T_PLUS||e->token.kind==T_MINUS){
       long long disp=e->token.kind==T_PLUS?imm:-imm;
       if(disp>=INT32_MIN&&disp<=INT32_MAX){
        if(disp>=0)text_add(o,"    lea rax, [%s + %lld]\n",reg,disp);
        else text_add(o,"    lea rax, [%s - %lld]\n",reg,-disp);
        break;
       }
      }
      if(e->token.kind==T_STAR){
       uint64_t magnitude=imm<0?(uint64_t)(-(imm+1))+1:(uint64_t)imm;
       if(magnitude&&!(magnitude&(magnitude-1))){
        text_add(o,"    mov rax, %s\n",reg);
        unsigned shift=0;while((UINT64_C(1)<<shift)!=magnitude)shift++;
        if(shift)text_add(o,"    shl rax, %u\n",shift);
        if(imm<0)text_add(o,"    neg rax\n");
       }else if(!magnitude)text_add(o,"    xor eax, eax\n");
       else text_add(o,"    imul rax, %s, %lld\n",reg,imm);
       break;
      }
     }
    }
    emit_expr(o,e->left,l,err);
    if(e->token.kind==T_PLUS){text_add(o,"    add rax, %lld\n",imm);break;}
    if(e->token.kind==T_MINUS){text_add(o,"    sub rax, %lld\n",imm);break;}
    if(e->token.kind==T_STAR){
     uint64_t magnitude=imm<0?(uint64_t)(-(imm+1))+1:(uint64_t)imm;
     if(!magnitude){text_add(o,"    xor eax, eax\n");break;}
     if(!(magnitude&(magnitude-1))){
      unsigned shift=0;while((UINT64_C(1)<<shift)!=magnitude)shift++;
      if(shift)text_add(o,"    shl rax, %u\n",shift);
      if(imm<0)text_add(o,"    neg rax\n");
     }else text_add(o,"    imul rax, rax, %lld\n",imm);
     break;
    }
    if(e->token.kind==T_EQ||e->token.kind==T_NE||e->token.kind==T_LT||e->token.kind==T_GT||e->token.kind==T_LE||e->token.kind==T_GE){
     const char *cc=e->token.kind==T_EQ?"sete":e->token.kind==T_NE?"setne":e->token.kind==T_LT?"setl":e->token.kind==T_GT?"setg":e->token.kind==T_LE?"setle":"setge";
     text_add(o,"    cmp rax, %lld\n    %s al\n    movzx rax, al\n",imm,cc);break;
    }
    if(e->token.kind==T_SLASH||e->token.kind==T_FLOORDIV){
     uint64_t divisor=imm<0?(uint64_t)(-(imm+1))+1:(uint64_t)imm;
     if(imm==1)break;
     if(divisor>1&&(divisor&(divisor-1))==0){
      unsigned shift=0;while((UINT64_C(1)<<shift)!=divisor)shift++;
      if(imm>0&&expr_is_nonnegative_midpoint(e,o->active_loop_condition))text_add(o,"    shr rax, %u\n",shift);
      else{text_add(o,"    mov rcx, rax\n    sar rcx, 63\n    and rcx, %llu\n    add rax, rcx\n    sar rax, %u\n",(unsigned long long)(divisor-1),shift);if(imm<0)text_add(o,"    neg rax\n");}
      break;
     }
    }
    if(e->token.kind==T_PERCENT&&imm!=0){
     uint64_t divisor=imm<0?(uint64_t)(-(imm+1))+1:(uint64_t)imm;
     if(divisor==1){text_add(o,"    xor eax, eax\n");break;}
     if((divisor&(divisor-1))==0){
      text_add(o,"    mov rcx, rax\n    sar rcx, 63\n    and rcx, %llu\n    add rax, rcx\n    and rax, %llu\n    sub rax, rcx\n",(unsigned long long)(divisor-1),(unsigned long long)(divisor-1));break;
     }
     uint64_t magic=UINT64_MAX/divisor;
     text_add(o,"    mov r10, rax\n    sar r10, 63\n    xor rax, r10\n    sub rax, r10\n    mov rcx, rax\n    movabs rdx, 0x%llx\n    mul rdx\n    imul rdx, rdx, %llu\n    sub rcx, rdx\n    lea rdx, [rcx - %llu]\n    cmp rcx, %llu\n    cmovae rcx, rdx\n    xor rcx, r10\n    sub rcx, r10\n    mov rax, rcx\n",
      (unsigned long long)magic,(unsigned long long)divisor,(unsigned long long)divisor,(unsigned long long)divisor);break;
    }
   }
  }
  if(e->right&&e->right->kind==E_NAME){
   Local *right=local_find(l,e->right->token.text);
   if(right){
    emit_expr(o,e->left,l,err);
    const char *src=right->reg>=0?cg_reg_name(right->reg):NULL;
    char memory[64];if(!src){snprintf(memory,sizeof(memory),"QWORD PTR [rbp - %d]",right->slot*8);src=memory;}
    switch(e->token.kind){
     case T_PLUS:text_add(o,"    add rax, %s\n",src);break;
     case T_MINUS:text_add(o,"    sub rax, %s\n",src);break;
     case T_STAR:text_add(o,"    imul rax, %s\n",src);break;
     case T_SLASH:case T_FLOORDIV:case T_PERCENT:
      text_add(o,"    mov rcx, %s\n    cqo\n    idiv rcx\n",src);
      if(e->token.kind==T_PERCENT)text_add(o,"    mov rax, rdx\n");break;
     case T_EQ:case T_NE:case T_LT:case T_GT:case T_LE:case T_GE:{
      const char *cc=e->token.kind==T_EQ?"sete":e->token.kind==T_NE?"setne":e->token.kind==T_LT?"setl":e->token.kind==T_GT?"setg":e->token.kind==T_LE?"setle":"setge";
      text_add(o,"    cmp rax, %s\n    %s al\n    movzx rax, al\n",src,cc);break;
     }
     default:goto cg_generic_binary;
    }
    break;
   }
  }
 cg_generic_binary:
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
 if(e&&e->kind==E_BINARY&&e->token.kind==T_AND){if(when_true){int id=o->next_label++;char skip[64];snprintf(skip,sizeof(skip),".Lswan_logic_skip_%d",id);emit_condition_jump(o,e->left,l,err,skip,false);emit_condition_jump(o,e->right,l,err,target,true);text_add(o,"%s:\n",skip);}else{emit_condition_jump(o,e->left,l,err,target,false);emit_condition_jump(o,e->right,l,err,target,false);}return;}
 if(e&&e->kind==E_BINARY&&e->token.kind==T_OR){if(when_true){emit_condition_jump(o,e->left,l,err,target,true);emit_condition_jump(o,e->right,l,err,target,true);}else{int id=o->next_label++;char skip[64];snprintf(skip,sizeof(skip),".Lswan_logic_skip_%d",id);emit_condition_jump(o,e->left,l,err,skip,true);emit_condition_jump(o,e->right,l,err,target,false);text_add(o,"%s:\n",skip);}return;}
 if(e&&e->kind==E_UNARY&&e->token.kind==T_NOT){emit_condition_jump(o,e->left,l,err,target,!when_true);return;}
 if(e&&e->kind==E_BINARY){
  const char *cc=NULL;
  switch(e->token.kind){
   case T_EQ:cc=when_true?"je":"jne";break;case T_NE:cc=when_true?"jne":"je";break;
   case T_LT:cc=when_true?"jl":"jge";break;case T_GT:cc=when_true?"jg":"jle";break;
   case T_LE:cc=when_true?"jle":"jg";break;case T_GE:cc=when_true?"jge":"jl";break;default:break;
  }
  if(cc){
    if((e->token.kind==T_EQ||e->token.kind==T_NE)&&e->left&&e->left->kind==E_BINARY&&e->left->token.kind==T_PERCENT&&e->left->right&&e->left->right->kind==E_INT&&!strcmp(e->left->right->token.text,"2")&&e->right&&e->right->kind==E_INT&&!strcmp(e->right->token.text,"0")&&e->left->left&&e->left->left->kind==E_NAME){
     Local *v=local_find(l,e->left->left->token.text);
     if(v){const char *jump=e->token.kind==T_EQ?(when_true?"jz":"jnz"):(when_true?"jnz":"jz");
      if(v->reg>=0)text_add(o,"    test %s, 1\n    %s %s\n",cg_reg_name(v->reg),jump,target);
      else text_add(o,"    test QWORD PTR [rbp - %d], 1\n    %s %s\n",v->slot*8,jump,target);
      return;
     }
    }
   if(e->left&&e->left->kind==E_NAME&&e->right&&e->right->kind==E_INT){
    char *end=NULL;long long imm=strtoll(e->right->token.text,&end,10);Local *v=local_find(l,e->left->token.text);
    if(v&&end&&!*end&&imm>=INT32_MIN&&imm<=INT32_MAX){
     if(v->reg>=0)text_add(o,"    cmp %s, %lld\n    %s %s\n",cg_reg_name(v->reg),imm,cc,target);
     else text_add(o,"    cmp QWORD PTR [rbp - %d], %lld\n    %s %s\n",v->slot*8,imm,cc,target);
     return;
    }
   }
   if(e->left&&e->left->kind==E_NAME&&e->right&&e->right->kind==E_NAME){
    Local *a=local_find(l,e->left->token.text),*b=local_find(l,e->right->token.text);
    if(a&&b){
     const char *ar=a->reg>=0?cg_reg_name(a->reg):NULL;
     const char *br=b->reg>=0?cg_reg_name(b->reg):NULL;
     char am[64],bm[64];
     if(!ar){snprintf(am,sizeof(am),"QWORD PTR [rbp - %d]",a->slot*8);ar=am;}
     if(!br){snprintf(bm,sizeof(bm),"QWORD PTR [rbp - %d]",b->slot*8);br=bm;}
     if(a->reg<0&&b->reg<0)text_add(o,"    mov rax, %s\n    cmp rax, %s\n    %s %s\n",ar,br,cc,target);
     else text_add(o,"    cmp %s, %s\n    %s %s\n",ar,br,cc,target);
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
 if(v->reg>=0)text_add(o,"    add %s, %lld\n",cg_reg_name(v->reg),imm);
 else text_add(o,"    add QWORD PTR [rbp - %d], %lld\n",v->slot*8,imm);
 return true;
}
static inline bool expr_writes_local(Expr *e,const char *name){
 if(!e)return false;
 if(e->kind==E_BINARY&&e->token.kind==T_ASSIGN&&e->left&&e->left->kind==E_NAME&&!strcmp(e->left->token.text,name))return true;
 if(expr_writes_local(e->left,name)||expr_writes_local(e->right,name))return true;
 for(size_t i=0;i<e->argc;i++)if(expr_writes_local(e->args[i],name))return true;
 return false;
}
static inline bool statements_write_local(Stmt **body,size_t n,const char *name){
 for(size_t i=0;i<n;i++){
  Stmt *s=body[i];
  if(s->kind==S_VARIABLE&&!strcmp(s->name,name))return true;
  if(expr_writes_local(s->value,name)||expr_writes_local(s->condition,name))return true;
  if(statements_write_local(s->body,s->body_count,name)||statements_write_local(s->alternate,s->alt_count,name))return true;
 }
 return false;
}
static inline bool expr_uses_local(Expr *e,const char *name){
 if(!e)return false;
 if(e->kind==E_NAME&&!strcmp(e->token.text,name))return true;
 if(expr_uses_local(e->left,name)||expr_uses_local(e->right,name))return true;
 for(size_t i=0;i<e->argc;i++)if(expr_uses_local(e->args[i],name))return true;
 return false;
}
static inline bool expr_hoist_safe(Expr *e,Locals *l){
 if(!e)return false;
 if(e->kind==E_INT||e->kind==E_BOOL)return true;
 if(e->kind==E_NAME)return local_find(l,e->token.text)!=NULL;
 if(e->kind==E_UNARY)return e->token.kind==T_MINUS&&expr_hoist_safe(e->left,l);
 if(e->kind==E_BINARY){
  if(e->token.kind!=T_PLUS&&e->token.kind!=T_MINUS&&e->token.kind!=T_STAR)return false;
  return expr_hoist_safe(e->left,l)&&expr_hoist_safe(e->right,l);
 }
 return false;
}
static inline bool expr_has_operator(Expr *e){
 return e&&(e->kind==E_UNARY||e->kind==E_BINARY);
}
static inline bool expr_names_stable_in_loop(Expr *e,Stmt **body,size_t n){
 if(!e)return true;
 if(e->kind==E_NAME&&statements_write_local(body,n,e->token.text))return false;
 if(!expr_names_stable_in_loop(e->left,body,n)||!expr_names_stable_in_loop(e->right,body,n))return false;
 for(size_t i=0;i<e->argc;i++)if(!expr_names_stable_in_loop(e->args[i],body,n))return false;
 return true;
}
static inline bool statements_have_call(Stmt **body,size_t n){
 for(size_t i=0;i<n;i++){
  Stmt *s=body[i];
  if(s->kind==S_LOG||expr_has_call(s->value)||expr_has_call(s->condition))return true;
  if(statements_have_call(s->body,s->body_count)||statements_have_call(s->alternate,s->alt_count))return true;
 }
 return false;
}
static inline bool statements_have_loop(Stmt **body,size_t n){
 for(size_t i=0;i<n;i++){
  Stmt *s=body[i];if(s->kind==S_WHILE)return true;
  if(statements_have_loop(s->body,s->body_count)||statements_have_loop(s->alternate,s->alt_count))return true;
 }
 return false;
}
static inline Expr *find_loop_invariant_expr(Expr *e,const char *induction,Stmt **body,size_t n,Locals *l){
 if(!e)return NULL;
 if(expr_hoist_safe(e,l)&&expr_has_operator(e)&&!expr_uses_local(e,induction)&&expr_names_stable_in_loop(e,body,n))return e;
 Expr *found=find_loop_invariant_expr(e->left,induction,body,n,l);if(found)return found;
 found=find_loop_invariant_expr(e->right,induction,body,n,l);if(found)return found;
 for(size_t i=0;i<e->argc;i++){found=find_loop_invariant_expr(e->args[i],induction,body,n,l);if(found)return found;}
 return NULL;
}
static inline Expr *find_loop_hoist(Stmt *s,Locals *l){
 if(!s||!s->condition||s->condition->kind!=E_BINARY||!s->condition->left||s->condition->left->kind!=E_NAME)return NULL;
 const char *induction=s->condition->left->token.text;
 if(statements_have_loop(s->body,s->body_count)||statements_have_call(s->body,s->body_count))return NULL;
 for(size_t i=0;i<s->body_count;i++){
  Stmt *item=s->body[i];
  Expr *found=find_loop_invariant_expr(item->value,induction,s->body,s->body_count,l);if(found)return found;
  found=find_loop_invariant_expr(item->condition,induction,s->body,s->body_count,l);if(found)return found;
 }
 return NULL;
}
static inline bool loop_bottom_test_candidate(Stmt *s,Locals *l);
static inline bool statements_have_early_exit(Stmt **body,size_t n){
 for(size_t i=0;i<n;i++){
  Stmt *s=body[i];if(s->kind==S_RETURN||s->kind==S_BREAK||s->kind==S_CONTINUE)return true;
  if(statements_have_early_exit(s->body,s->body_count)||statements_have_early_exit(s->alternate,s->alt_count))return true;
 }
 return false;
}
static inline Expr *find_induction_product(Expr *e,const char *name,long long *coefficient){
 if(!e)return NULL;
 if(e->kind==E_BINARY&&e->token.kind==T_STAR&&e->left&&e->left->kind==E_NAME&&!strcmp(e->left->token.text,name)&&e->right&&e->right->kind==E_INT){
  char *end=NULL;long long value=strtoll(e->right->token.text,&end,10);
  if(end&&!*end&&value>=INT32_MIN&&value<=INT32_MAX&&value!=0){*coefficient=value;return e;}
 }
 Expr *found=find_induction_product(e->left,name,coefficient);if(found)return found;
 found=find_induction_product(e->right,name,coefficient);if(found)return found;
 for(size_t i=0;i<e->argc;i++){found=find_induction_product(e->args[i],name,coefficient);if(found)return found;}
 return NULL;
}
static inline bool local_uses_reg(Locals *l,int reg){for(size_t i=0;i<l->count;i++)if(l->items[i].reg==reg)return true;return false;}
static inline Expr *find_loop_recurrence(Stmt *s,Locals *l,long long *coefficient){
 if(!loop_bottom_test_candidate(s,l)||local_uses_reg(l,4)||statements_have_loop(s->body,s->body_count)||statements_have_call(s->body,s->body_count)||statements_have_early_exit(s->body,s->body_count))return NULL;
 const char *name=s->condition->left->token.text;
 for(size_t i=0;i<s->body_count;i++){
  Stmt *item=s->body[i];
  Expr *found=find_induction_product(item->value,name,coefficient);if(found)return found;
  found=find_induction_product(item->condition,name,coefficient);if(found)return found;
 }
 return NULL;
}
static inline void emit_recurrence_init(Text *o,Locals *l,const char *name,long long coefficient){
 Local *v=local_find(l,name);
 if(v->reg>=0)text_add(o,"    imul r15, %s, %lld\n",cg_reg_name(v->reg),coefficient);
 else text_add(o,"    imul r15, QWORD PTR [rbp - %d], %lld\n",v->slot*8,coefficient);
}
static inline bool loop_bottom_test_candidate(Stmt *s,Locals *l){
 if(!s||s->kind!=S_WHILE||statements_have_early_exit(s->body,s->body_count)||!s->condition||s->condition->kind!=E_BINARY)return false;
 Expr *cond=s->condition;
 if((cond->token.kind!=T_LT&&cond->token.kind!=T_LE)||!cond->left||cond->left->kind!=E_NAME||!cond->right||cond->right->kind!=E_INT)return false;
 char *end=NULL;long long bound=strtoll(cond->right->token.text,&end,10);
 if(!end||*end||bound<INT32_MIN||bound>INT32_MAX||!local_find(l,cond->left->token.text))return false;
 if(!s->body_count)return false;
 Stmt *last=s->body[s->body_count-1];
 if(last->kind!=S_EXPR||!last->value||last->value->kind!=E_BINARY||last->value->token.kind!=T_ASSIGN)return false;
 Expr *assign=last->value;
 if(!assign->left||assign->left->kind!=E_NAME||strcmp(assign->left->token.text,cond->left->token.text))return false;
 Expr *step=assign->right;
 if(!step||step->kind!=E_BINARY||step->token.kind!=T_PLUS||!step->left||step->left->kind!=E_NAME||strcmp(step->left->token.text,cond->left->token.text)||!step->right||step->right->kind!=E_INT||strcmp(step->right->token.text,"1"))return false;
 return !statements_write_local(s->body,s->body_count-1,cond->left->token.text);
}
static inline bool midpoint_expr_writes_name(Expr *e,const char *name){
 if(!e)return false;
 if(e->kind==E_BINARY&&e->token.kind==T_ASSIGN&&e->left&&e->left->kind==E_NAME&&!strcmp(e->left->token.text,name))return true;
 if(midpoint_expr_writes_name(e->left,name)||midpoint_expr_writes_name(e->right,name))return true;
 for(size_t i=0;i<e->argc;i++)if(midpoint_expr_writes_name(e->args[i],name))return true;
 return false;
}
static inline bool midpoint_stmt_writes_name(Stmt *s,const char *name){
 if(!s)return false;
 if(s->kind==S_VARIABLE&&s->name&&!strcmp(s->name,name))return true;
 if(midpoint_expr_writes_name(s->value,name)||midpoint_expr_writes_name(s->condition,name))return true;
 for(size_t i=0;i<s->body_count;i++)if(midpoint_stmt_writes_name(s->body[i],name))return true;
 for(size_t i=0;i<s->alt_count;i++)if(midpoint_stmt_writes_name(s->alternate[i],name))return true;
 return false;
}
static inline bool midpoint_block_init_nonnegative(Stmt **body,size_t before,const char *name){
 bool found=false;int64_t value=0;
 for(size_t i=0;i<before;i++){
  Stmt *s=body[i];
  if(s->kind==S_VARIABLE&&s->name&&!strcmp(s->name,name)){found=cg_const_value(s->value,&value)&&value>=0&&value<INT64_MAX;continue;}
  if(midpoint_stmt_writes_name(s,name))return false;
 }
 return found;
}
static inline bool midpoint_step_expr(Expr *e,const char *middle,TokenKind op){
 return e&&e->kind==E_BINARY&&e->token.kind==op&&e->left&&e->left->kind==E_NAME&&!strcmp(e->left->token.text,middle)&&e->right&&e->right->kind==E_INT&&!strcmp(e->right->token.text,"1");
}
static inline bool midpoint_loop_updates(Stmt **body,size_t count,const char *target,const char *middle,TokenKind op,bool *found){
 for(size_t i=0;i<count;i++){
  Stmt *s=body[i];
  if(s->kind==S_EXPR&&s->value&&s->value->kind==E_BINARY&&s->value->token.kind==T_ASSIGN&&s->value->left&&s->value->left->kind==E_NAME&&!strcmp(s->value->left->token.text,target)){
   if(!midpoint_step_expr(s->value->right,middle,op))return false;
   *found=true;continue;
  }
  if(s->kind==S_WHILE&&midpoint_stmt_writes_name(s,target))return false;
  if(s->kind==S_IF){
   if(midpoint_expr_writes_name(s->condition,target)||!midpoint_loop_updates(s->body,s->body_count,target,middle,op,found)||!midpoint_loop_updates(s->alternate,s->alt_count,target,middle,op,found))return false;
  }else if(midpoint_expr_writes_name(s->value,target)||midpoint_expr_writes_name(s->condition,target))return false;
 }
 return true;
}
static inline bool loop_proves_nonnegative_midpoint(Stmt **outer,size_t before,Stmt *loop){
 if(!loop||loop->kind!=S_WHILE||!loop->condition||loop->condition->kind!=E_BINARY||loop->condition->token.kind!=T_LE||!loop->condition->left||loop->condition->left->kind!=E_NAME||!loop->condition->right||loop->condition->right->kind!=E_NAME||!loop->body_count||!stmt_is_safe_midpoint(loop->body[0],loop->condition))return false;
 const char *low=loop->condition->left->token.text,*high=loop->condition->right->token.text,*middle=loop->body[0]->name;
 if(!middle||!midpoint_block_init_nonnegative(outer,before,low)||!midpoint_block_init_nonnegative(outer,before,high))return false;
 for(size_t i=1;i<loop->body_count;i++)if(midpoint_stmt_writes_name(loop->body[i],middle))return false;
 bool low_updated=false,high_updated=false;
 return midpoint_loop_updates(loop->body,loop->body_count,low,middle,T_PLUS,&low_updated)&&midpoint_loop_updates(loop->body,loop->body_count,high,middle,T_MINUS,&high_updated);
}
static inline void emit_block(Text *o,Stmt **body,size_t n,Locals *l,int *label,SwanError *err){
 for(size_t i=0;i<n;i++){
  Stmt *s=body[i];
  switch(s->kind){
   case S_VARIABLE:{
    Local *v=local_find(l,s->name);long long value=0;bool immediate=!s->value;
    if(s->value&&s->value->kind==E_INT){char *end=NULL;value=strtoll(s->value->token.text,&end,10);immediate=end&&!*end;}
    else if(s->value&&s->value->kind==E_BOOL){value=s->value->token.kind==T_TRUE;immediate=true;}
    if(v&&immediate){
     if(v->reg>=0){
       const char *low=cg_reg_low_name(v->reg),*reg=cg_reg_name(v->reg);
       if(value==0)text_add(o,"    xor %s, %s\n",low,low);
       else if(value>=INT32_MIN&&value<=INT32_MAX)text_add(o,"    mov %s, %lld\n",reg,value);
       else text_add(o,"    movabs %s, %lld\n",reg,value);
     }else if(value>=INT32_MIN&&value<=INT32_MAX)text_add(o,"    mov QWORD PTR [rbp - %d], %lld\n",v->slot*8,value);
     else{if(s->value)emit_expr(o,s->value,l,err);else text_add(o,"    xor eax, eax\n");text_add(o,"    mov QWORD PTR [rbp - %d], rax\n",v->slot*8);}
    }else{
     if(s->value)emit_expr(o,s->value,l,err);else text_add(o,"    xor eax, eax\n");
     if(v){if(v->reg>=0)text_add(o,"    mov %s, rax\n",cg_reg_name(v->reg));else text_add(o,"    mov QWORD PTR [rbp - %d], rax\n",v->slot*8);}
    }
    break;
   }
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
     cg_save_volatile(o,l);text_add(o,"\"\n.text\n    lea rdi, [rip + .Lswan_str_%d]\n",id);cg_call(o,"puts@PLT");cg_restore_volatile(o,l);
    }else{emit_expr(o,s->value,l,err);cg_save_volatile(o,l);text_add(o,"    mov rsi, rax\n    lea rdi, [rip + .Lswan_fmt]\n    xor eax, eax\n");cg_call(o,"printf@PLT");cg_restore_volatile(o,l);}
    break;
    case S_RETURN:if(s->value)emit_expr(o,s->value,l,err);else text_add(o,"    xor eax, eax\n");emit_restore_regs(o,l);if(o->frameless)text_add(o,"    ret\n");else text_add(o,"    leave\n    ret\n");break;
   case S_EXPR:if(!emit_accumulate_statement(o,s->value,l,err)&&!emit_increment_statement(o,s->value,l))emit_expr(o,s->value,l,err);break;
   case S_PASS:break;
   case S_BREAK:if(!o->loop_depth)swan_error(err,&s->token,"break is only valid inside a loop");else text_add(o,"    jmp .Lswan_loop_end_%d\n",o->loop_break[o->loop_depth-1]);break;
   case S_CONTINUE:if(!o->loop_depth)swan_error(err,&s->token,"continue is only valid inside a loop");else text_add(o,"    jmp .Lswan_loop_%d\n",o->loop_continue[o->loop_depth-1]);break;
   case S_IF:{
    int id=(*label)++;char target[64];snprintf(target,sizeof(target),".Lswan_else_%d",id);emit_condition_jump(o,s->condition,l,err,target,false);
    emit_block(o,s->body,s->body_count,l,label,err);text_add(o,"    jmp .Lswan_end_%d\n.Lswan_else_%d:\n",id,id);
    emit_block(o,s->alternate,s->alt_count,l,label,err);text_add(o,".Lswan_end_%d:\n",id);break;
   }
   case S_WHILE:{
    int id=(*label)++;char target[64];snprintf(target,sizeof(target),".Lswan_loop_end_%d",id);
    if(o->loop_depth>=128){swan_error(err,&s->token,"loop nesting exceeds 128 levels");break;}o->loop_break[o->loop_depth]=id;o->loop_continue[o->loop_depth]=id;o->loop_depth++;
    Expr *saved_hoist=o->hoist_expr,*saved_recurrence=o->recurrence_expr,*saved_active=o->active_loop_condition;o->active_loop_condition=loop_proves_nonnegative_midpoint(body,i,s)?s->condition:NULL;
    Expr *hoist=saved_hoist?NULL:find_loop_hoist(s,l);
    if(hoist){emit_expr(o,hoist,l,err);text_add(o,"    mov r11, rax\n");o->hoist_expr=hoist;}
    long long recurrence_step=0;
    Expr *recurrence=saved_recurrence?NULL:find_loop_recurrence(s,l,&recurrence_step);
    if(recurrence){
     cg_push(o,"r15");
     emit_recurrence_init(o,l,s->condition->left->token.text,recurrence_step);
     o->recurrence_expr=recurrence;
    }
    if(loop_bottom_test_candidate(s,l)){
     emit_condition_jump(o,s->condition,l,err,target,false);
     text_add(o,".Lswan_loop_%d:\n",id);
     emit_block(o,s->body,s->body_count,l,label,err);
     if(recurrence)text_add(o,"    add r15, %lld\n",recurrence_step);
     snprintf(target,sizeof(target),".Lswan_loop_%d",id);
     emit_condition_jump(o,s->condition,l,err,target,true);
     text_add(o,".Lswan_loop_end_%d:\n",id);
    }else{
     text_add(o,".Lswan_loop_%d:\n",id);emit_condition_jump(o,s->condition,l,err,target,false);emit_block(o,s->body,s->body_count,l,label,err);
     text_add(o,"    jmp .Lswan_loop_%d\n.Lswan_loop_end_%d:\n",id,id);
    }
    o->hoist_expr=saved_hoist;o->recurrence_expr=saved_recurrence;o->active_loop_condition=saved_active;
    if(recurrence)cg_pop(o,"r15");
    o->loop_depth--;
    break;
   }
   default:break;
  }
 }
}
static inline bool swan_codegen_x86_64(Program *p,Text *o,SwanError *err){
 text_add(o,".intel_syntax noprefix\n.text\n");o->program=p;int strid=0;
 for(size_t i=0;i<p->count;i++){
  Stmt *fn=p->functions[i];Locals l={0};o->stack_depth=0;
  l.caller_saved=!statements_have_call(fn->body,fn->body_count);
  for(size_t a=0;a<fn->param_count;a++)local_add(&l,fn->params[a],false);
  collect_locals(fn->body,fn->body_count,&l);int frame=((l.slots+l.saved_reg_count)*8+15)/16*16;
  char sym[SWAN_MAX_TEXT+16];if(!strcmp(fn->name,"main"))strcpy(sym,"main");else snprintf(sym,sizeof(sym),"swan_fn_%s",fn->name);
  o->frameless=frame==0&&l.saved_reg_count==0;if(o->frameless)text_add(o,".globl %s\n%s:\n",sym,sym);else{text_add(o,".globl %s\n%s:\n    push rbp\n    mov rbp, rsp\n",sym,sym);if(frame)text_add(o,"    sub rsp, %d\n",frame);}
  for(int a=0;a<l.saved_reg_count;a++)text_add(o,"    mov QWORD PTR [rbp - %d], %s\n",l.slots*8+(a+1)*8,cg_saved_regs[a]);
  const char *regs[]={"rdi","rsi","rdx","rcx","r8","r9"};
  for(size_t a=fn->param_count<6?fn->param_count:6;a>0;a--){size_t index=a-1;Local *v=local_find(&l,fn->params[index]);if(v){if(v->reg>=0){const char *dst=cg_reg_name(v->reg);if(strcmp(dst,regs[index]))text_add(o,"    mov %s, %s\n",dst,regs[index]);}else text_add(o,"    mov QWORD PTR [rbp - %d], %s\n",v->slot*8,regs[index]);}}
  emit_block(o,fn->body,fn->body_count,&l,&strid,err);text_add(o,"    xor eax, eax\n");emit_restore_regs(o,&l);if(o->frameless)text_add(o,"    ret\n");else text_add(o,"    leave\n    ret\n");
 }
 if(o->uses_array_bounds)text_add(o,".text\n.Lswan_array_bounds_fail:\n    call abort@PLT\n    ud2\n");
 text_add(o,".section .rodata\n.Lswan_fmt:\n    .string \"%%ld\\n\"\n.section .note.GNU-stack,\"\",@progbits\n");
 if(o->failed){swan_error(err,NULL,"out of memory while generating assembly");return false;}
 return err->message[0]==0;
}
#endif
