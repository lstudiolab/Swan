#ifndef SWAN_SEMA_H
#define SWAN_SEMA_H
#include "swan/parser.h"
typedef struct { char name[SWAN_MAX_TEXT]; bool mutable_value; int previous; } SwanSymbol;
typedef struct { SwanSymbol *items; size_t count; int buckets[2048]; } SwanSymbols;
static inline unsigned swan_name_hash(const char *s){
 uint32_t h=2166136261u;for(;*s;s++)h=(h^(unsigned char)*s)*16777619u;return h;
}
static inline Stmt *swan_find_function(Program *p,const char *name){
 if(p->function_indexed){unsigned b=swan_name_hash(name)&2047u;for(int n=p->function_buckets[b];n;n=p->function_next[n-1]){Stmt *fn=p->functions[n-1];if(!strcmp(fn->name,name))return fn;}return NULL;}
 for(size_t i=0;i<p->count;i++)if(!strcmp(p->functions[i]->name,name))return p->functions[i];return NULL;
}
static inline bool swan_symbol_find(SwanSymbols *s,const char *name,size_t *index){
 unsigned b=swan_name_hash(name)&2047u;for(int n=s->buckets[b];n;n=s->items[n-1].previous){if(!strcmp(s->items[n-1].name,name)){if(index)*index=(size_t)(n-1);return true;}}return false;
}
static inline bool swan_symbol_add(SwanSymbols *s,const char *name,bool mut){
 if(s->count>=1024)return false;unsigned b=swan_name_hash(name)&2047u;size_t i=s->count++;
 SwanSymbol *v=&s->items[i];strcpy(v->name,name);v->mutable_value=mut;v->previous=s->buckets[b];s->buckets[b]=(int)i+1;return true;
}
static inline void swan_symbols_leave(SwanSymbols *s,size_t mark){
 while(s->count>mark){size_t i=--s->count;unsigned b=swan_name_hash(s->items[i].name)&2047u;s->buckets[b]=s->items[i].previous;}
}
static inline bool swan_check_expr(Program *p,Expr *e,SwanSymbols *symbols,SwanError *err){
 if(!e)return true;
 switch(e->kind){
  case E_NAME:if(!swan_symbol_find(symbols,e->token.text,NULL)){swan_error(err,&e->token,"unknown name '%s'",e->token.text);return false;}return true;
  case E_CALL:{
   if(!e->left||e->left->kind!=E_NAME){swan_error(err,&e->token,"only direct function calls are supported");return false;}
   if(!strcmp(e->left->token.text,"array")){swan_error(err,&e->token,"array allocation is not yet ported to the C backend");return false;}
   Stmt *fn=swan_find_function(p,e->left->token.text);
   if(!fn){swan_error(err,&e->token,"unknown function '%s'",e->left->token.text);return false;}
   if(fn->param_count!=e->argc){swan_error(err,&e->token,"function '%s' expects %zu arguments, got %zu",fn->name,fn->param_count,e->argc);return false;}
   for(size_t i=0;i<e->argc;i++)if(!swan_check_expr(p,e->args[i],symbols,err))return false;
   return true;
  }
  case E_UNARY:return swan_check_expr(p,e->left,symbols,err);
  case E_BINARY:
   if(e->token.kind==T_ASSIGN){
    if(!e->left||e->left->kind!=E_NAME){swan_error(err,&e->token,"assignment target must be a variable");return false;}
    size_t index;if(!swan_symbol_find(symbols,e->left->token.text,&index)){swan_error(err,&e->left->token,"unknown variable '%s'",e->left->token.text);return false;}
    if(!symbols->items[index].mutable_value){swan_error(err,&e->left->token,"cannot assign to immutable value '%s'",e->left->token.text);return false;}
    return swan_check_expr(p,e->right,symbols,err);
   }
   return swan_check_expr(p,e->left,symbols,err)&&swan_check_expr(p,e->right,symbols,err);
  default:return true;
 }
}
static inline bool swan_check_block(Program *p,Stmt **body,size_t count,SwanSymbols *symbols,SwanError *err){
 for(size_t i=0;i<count;i++){
  Stmt *s=body[i];
  switch(s->kind){
   case S_VARIABLE:
    if(swan_symbol_find(symbols,s->name,NULL)){swan_error(err,&s->token,"'%s' is already declared",s->name);return false;}
    if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;
    if(!swan_symbol_add(symbols,s->name,s->mutable_value)){swan_error(err,&s->token,"too many local variables");return false;}break;
   case S_LOG:case S_RETURN:case S_EXPR:if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;break;
   case S_IF:case S_WHILE:{
    if(!swan_check_expr(p,s->condition,symbols,err))return false;
    size_t mark=symbols->count;
    if(!swan_check_block(p,s->body,s->body_count,symbols,err)){swan_symbols_leave(symbols,mark);return false;}
    swan_symbols_leave(symbols,mark);mark=symbols->count;
    if(!swan_check_block(p,s->alternate,s->alt_count,symbols,err)){swan_symbols_leave(symbols,mark);return false;}
    swan_symbols_leave(symbols,mark);break;
   }
   case S_BREAK:case S_CONTINUE:swan_error(err,&s->token,"break and continue lowering are not yet ported to the C backend");return false;
   default:break;
  }
 }
 return true;
}
static inline bool swan_semantic_check(Program *p,SwanError *err){
 memset(p->function_buckets,0,sizeof(p->function_buckets));p->function_indexed=true;
 for(size_t i=0;i<p->count;i++){
  unsigned b=swan_name_hash(p->functions[i]->name)&2047u;
  for(int n=p->function_buckets[b];n;n=p->function_next[n-1])if(!strcmp(p->functions[n-1]->name,p->functions[i]->name)){swan_error(err,&p->functions[i]->token,"function '%s' is already declared",p->functions[i]->name);return false;}
  p->function_next[i]=(unsigned short)p->function_buckets[b];p->function_buckets[b]=(int)i+1;
 }
 if(!swan_find_function(p,"main")){swan_error(err,NULL,"program must define fn main");return false;}
 SwanSymbols symbols={0};symbols.items=calloc(1024,sizeof(*symbols.items));if(!symbols.items){swan_error(err,NULL,"out of memory allocating semantic symbol table");return false;}
 bool ok=true;
 for(size_t i=0;i<p->count&&ok;i++){
  Stmt *fn=p->functions[i];symbols.count=0;memset(symbols.buckets,0,sizeof(symbols.buckets));
  for(size_t a=0;a<fn->param_count;a++){
   if(swan_symbol_find(&symbols,fn->params[a],NULL)){swan_error(err,&fn->token,"duplicate parameter '%s'",fn->params[a]);ok=false;break;}
   if(!swan_symbol_add(&symbols,fn->params[a],false)){swan_error(err,&fn->token,"too many parameters");ok=false;break;}
  }
  if(ok)ok=swan_check_block(p,fn->body,fn->body_count,&symbols,err);
 }
 free(symbols.items);return ok;
}
#endif
