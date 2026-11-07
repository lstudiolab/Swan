#ifndef SWAN_SEMA_H
#define SWAN_SEMA_H
#include "swan/parser.h"
typedef struct { char names[1024][SWAN_MAX_TEXT]; bool mutable_values[1024]; size_t count; } SwanSymbols;
static inline Stmt *swan_find_function(Program *p,const char *name){for(size_t i=0;i<p->count;i++)if(!strcmp(p->functions[i]->name,name))return p->functions[i];return NULL;}
static inline bool swan_symbol_find(SwanSymbols *s,const char *name,size_t *index){for(size_t i=s->count;i>0;i--)if(!strcmp(s->names[i-1],name)){if(index)*index=i-1;return true;}return false;}
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
    if(e->left->kind!=E_NAME){swan_error(err,&e->token,"assignment target must be a variable");return false;}
    size_t index;if(!swan_symbol_find(symbols,e->left->token.text,&index)){swan_error(err,&e->left->token,"unknown variable '%s'",e->left->token.text);return false;}
    if(!symbols->mutable_values[index]){swan_error(err,&e->left->token,"cannot assign to immutable value '%s'",e->left->token.text);return false;}
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
    if(symbols->count>=1024){swan_error(err,&s->token,"too many local variables");return false;}
    strcpy(symbols->names[symbols->count],s->name);symbols->mutable_values[symbols->count]=s->mutable_value;symbols->count++;break;
   case S_LOG:case S_RETURN:case S_EXPR:if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;break;
   case S_IF:case S_WHILE:{
    if(!swan_check_expr(p,s->condition,symbols,err))return false;
    SwanSymbols nested=*symbols;
    if(!swan_check_block(p,s->body,s->body_count,&nested,err))return false;
    nested=*symbols;
    if(!swan_check_block(p,s->alternate,s->alt_count,&nested,err))return false;
    break;
   }
   case S_BREAK:case S_CONTINUE:swan_error(err,&s->token,"break and continue lowering are not yet ported to the C backend");return false;
   default:break;
  }
 }
 return true;
}
static inline bool swan_semantic_check(Program *p,SwanError *err){
 if(!swan_find_function(p,"main")){swan_error(err,NULL,"program must define fn main");return false;}
 for(size_t i=0;i<p->count;i++){
  Stmt *fn=p->functions[i];
  for(size_t j=0;j<i;j++)if(!strcmp(p->functions[j]->name,fn->name)){swan_error(err,&fn->token,"function '%s' is already declared",fn->name);return false;}
  SwanSymbols symbols={0};
  for(size_t a=0;a<fn->param_count;a++){
   if(swan_symbol_find(&symbols,fn->params[a],NULL)){swan_error(err,&fn->token,"duplicate parameter '%s'",fn->params[a]);return false;}
   strcpy(symbols.names[symbols.count],fn->params[a]);symbols.mutable_values[symbols.count]=false;symbols.count++;
  }
  if(!swan_check_block(p,fn->body,fn->body_count,&symbols,err))return false;
 }
 return true;
}
#endif
