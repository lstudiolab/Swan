#ifndef SWAN_SEMA_H
#define SWAN_SEMA_H
#include "swan/parser.h"
typedef struct { const char *name; bool mutable_value,manual_memory,manual_checked,manual_freed; int previous; } SwanSymbol;
typedef struct { SwanSymbol *items; size_t count; int buckets[2048]; } SwanSymbols;
static inline unsigned swan_name_hash(const char *s){
 uint32_t h=2166136261u;for(;*s;s++)h=(h^(unsigned char)*s)*16777619u;return h;
}
static inline Stmt *swan_find_function(Program *p,const char *name){
 if(p->function_indexed){unsigned b=swan_name_hash(name)&2047u;for(int n=p->function_buckets[b];n;n=p->function_next[n-1]){Stmt *fn=p->functions[n-1];if(!strcmp(fn->name,name))return fn;}return NULL;}
 for(size_t i=0;i<p->count;i++)if(!strcmp(p->functions[i]->name,name))return p->functions[i];
 return NULL;
}
static inline bool swan_symbol_find(SwanSymbols *s,const char *name,size_t *index){
 unsigned b=swan_name_hash(name)&2047u;for(int n=s->buckets[b];n;n=s->items[n-1].previous){if(!strcmp(s->items[n-1].name,name)){if(index)*index=(size_t)(n-1);return true;}}return false;
}
static inline bool swan_symbol_add(SwanSymbols *s,const char *name,bool mut,bool manual,bool checked){
 if(s->count>=1024)return false;
 unsigned b=swan_name_hash(name)&2047u;size_t i=s->count++;
 SwanSymbol *v=&s->items[i];v->name=name;v->mutable_value=mut;v->manual_memory=manual;v->manual_checked=checked;v->manual_freed=false;v->previous=s->buckets[b];s->buckets[b]=(int)i+1;return true;
}
static inline void swan_symbols_leave(SwanSymbols *s,size_t mark){
 while(s->count>mark){size_t i=--s->count;unsigned b=swan_name_hash(s->items[i].name)&2047u;s->buckets[b]=s->items[i].previous;}
}
static inline bool swan_check_expr(Program *p,Expr *e,SwanSymbols *symbols,SwanError *err){
 if(!e)return true;
 switch(e->kind){
  case E_INT:case E_FLOAT:case E_STRING:case E_BOOL:return true;
  case E_NAME:{size_t index;if(!swan_symbol_find(symbols,e->token.text,&index)){swan_error(err,&e->token,"unknown name '%s'",e->token.text);return false;}SwanSymbol *v=&symbols->items[index];if(v->manual_checked){if(v->manual_freed){swan_error(err,&e->token,"use of manually freed variable '%s'",e->token.text);return false;}swan_error(err,&e->token,"manual allocation handles may only be indexed or released with free");return false;}return true;}
  case E_CALL:{
   const char *callee=swan_expr_terminal_name(e->left);if(!callee){swan_error(err,&e->token,"call target must be a function name or dotted function path");return false;}
   if(!strcmp(callee,"array")||!strcmp(callee,"array_u32")||!strcmp(callee,"array_unchecked")||!strcmp(callee,"array_u32_unchecked")){
    if(e->argc!=1){swan_error(err,&e->token,"array_u32 expects exactly one length argument");return false;}
    return swan_check_expr(p,e->args[0],symbols,err);
   }
   Stmt *fn=swan_find_function(p,callee);
   if(!fn){swan_error(err,&e->token,"unknown function '%s'",callee);return false;}
   if(fn->param_count!=e->argc){swan_error(err,&e->token,"function '%s' expects %zu arguments, got %zu",fn->name,fn->param_count,e->argc);return false;}
   for(size_t i=0;i<e->argc;i++){
    if(e->args[i]&&e->args[i]->kind==E_NAME){size_t arg_index;if(swan_symbol_find(symbols,e->args[i]->token.text,&arg_index)&&symbols->items[arg_index].manual_checked){swan_error(err,&e->args[i]->token,"manual ownership cannot be copied into a function call");return false;}}
    if(!swan_check_expr(p,e->args[i],symbols,err))return false;
   }
   return true;
  }
  case E_MEMBER:swan_error(err,&e->token,"member values require struct or module value resolution, which is not implemented yet");return false;
  case E_INDEX:
   if(!e->left||e->left->kind!=E_NAME||!swan_symbol_find(symbols,e->left->token.text,NULL)){
    swan_error(err,&e->token,"array indexing requires a declared array variable");return false;
   }
   {size_t base_index;if(swan_symbol_find(symbols,e->left->token.text,&base_index)&&symbols->items[base_index].manual_checked&&symbols->items[base_index].manual_freed){swan_error(err,&e->left->token,"use of manually freed variable '%s'",e->left->token.text);return false;}}
   return swan_check_expr(p,e->right,symbols,err);
  case E_UNARY:return swan_check_expr(p,e->left,symbols,err);
  case E_BINARY:
   if(e->token.kind==T_ASSIGN){
    if(e->right&&e->right->kind==E_NAME){size_t right_index;if(swan_symbol_find(symbols,e->right->token.text,&right_index)&&symbols->items[right_index].manual_checked){swan_error(err,&e->right->token,"manual ownership cannot be copied or aliased");return false;}}
    if(e->left&&e->left->kind==E_INDEX){
     if(!e->left->left||e->left->left->kind!=E_NAME||!swan_symbol_find(symbols,e->left->left->token.text,NULL)){
      swan_error(err,&e->token,"array assignment requires a declared array variable");return false;
     }
     {size_t base_index;if(swan_symbol_find(symbols,e->left->left->token.text,&base_index)&&symbols->items[base_index].manual_checked&&symbols->items[base_index].manual_freed){swan_error(err,&e->left->left->token,"use of manually freed variable '%s'",e->left->left->token.text);return false;}}
     return swan_check_expr(p,e->left->right,symbols,err)&&swan_check_expr(p,e->right,symbols,err);
    }
    if(!e->left||e->left->kind!=E_NAME){swan_error(err,&e->token,"assignment target must be a variable or array element");return false;}
    size_t index;if(!swan_symbol_find(symbols,e->left->token.text,&index)){swan_error(err,&e->left->token,"unknown variable '%s'",e->left->token.text);return false;}
    if(symbols->items[index].manual_checked){swan_error(err,&e->left->token,"manual variables cannot be reassigned or moved");return false;}
    if(!symbols->items[index].mutable_value){swan_error(err,&e->left->token,"cannot assign to immutable value '%s'",e->left->token.text);return false;}
    return swan_check_expr(p,e->right,symbols,err);
   }
   return swan_check_expr(p,e->left,symbols,err)&&swan_check_expr(p,e->right,symbols,err);
  default:swan_error(err,&e->token,"expression kind %d is not implemented by semantic analysis",(int)e->kind);return false;
 }
}
static inline bool swan_check_block(Program *p,Stmt **body,size_t count,SwanSymbols *symbols,SwanError *err,unsigned loop_depth){
 size_t scope_mark=symbols->count;
 for(size_t i=0;i<count;i++){
  Stmt *s=body[i];
  switch(s->kind){
   case S_VARIABLE:{
    if(s->manual_checked&&(!s->mutable_value||!s->value||s->value->kind!=E_CALL||!s->value->left||s->value->left->kind!=E_NAME||
      (strcmp(s->value->left->token.text,"array")&&strcmp(s->value->left->token.text,"array_u32")&&strcmp(s->value->left->token.text,"array_unchecked")&&strcmp(s->value->left->token.text,"array_u32_unchecked")))){
     swan_error(err,&s->token,"manual var must initialize a mutable array allocation");return false;
    }
    if(s->value&&s->value->kind==E_NAME){size_t source;if(swan_symbol_find(symbols,s->value->token.text,&source)&&symbols->items[source].manual_checked){swan_error(err,&s->value->token,"manual ownership cannot be copied or aliased");return false;}}
    if(swan_symbol_find(symbols,s->name,NULL)){swan_error(err,&s->token,"'%s' is already declared",s->name);return false;}
    if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;
    if(!swan_symbol_add(symbols,s->name,s->mutable_value,s->manual_memory,s->manual_checked)){swan_error(err,&s->token,"too many local variables");return false;}
    break;
   }
   case S_LOG:
    if(s->value&&s->value->kind==E_NAME){size_t index;if(swan_symbol_find(symbols,s->value->token.text,&index)&&symbols->items[index].manual_checked){swan_error(err,&s->value->token,"manual allocation handles cannot be printed or copied");return false;}}
    if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;
    break;
   case S_RETURN:
    if(s->value&&s->value->kind==E_NAME){size_t index;if(swan_symbol_find(symbols,s->value->token.text,&index)&&symbols->items[index].manual_memory){swan_error(err,&s->value->token,"manual ownership cannot escape through a return value");return false;}}
    if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;
    for(size_t k=0;k<symbols->count;k++)if(symbols->items[k].manual_checked&&!symbols->items[k].manual_freed){swan_error(err,&s->token,"manual variable '%s' must be freed before returning",symbols->items[k].name);return false;}
    break;
   case S_EXPR:
    if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;
    break;
   case S_IF:{
    if(!swan_check_expr(p,s->condition,symbols,err))return false;
    size_t outer=symbols->count;bool before[1024],then_state[1024],else_state[1024];
    for(size_t k=0;k<outer;k++)before[k]=symbols->items[k].manual_freed;
    if(!swan_check_block(p,s->body,s->body_count,symbols,err,loop_depth)){swan_symbols_leave(symbols,outer);return false;}
    for(size_t k=0;k<outer;k++)then_state[k]=symbols->items[k].manual_freed;
    swan_symbols_leave(symbols,outer);
    for(size_t k=0;k<outer;k++)symbols->items[k].manual_freed=before[k];
    if(!swan_check_block(p,s->alternate,s->alt_count,symbols,err,loop_depth)){swan_symbols_leave(symbols,outer);return false;}
    for(size_t k=0;k<outer;k++)else_state[k]=symbols->items[k].manual_freed;
    swan_symbols_leave(symbols,outer);
    for(size_t k=0;k<outer;k++){
     if(symbols->items[k].manual_checked&&then_state[k]!=else_state[k]){
      swan_error(err,&s->token,"manual variable '%s' must have the same ownership state on every branch",symbols->items[k].name);return false;
     }
     symbols->items[k].manual_freed=then_state[k];
    }
    break;
   }
   case S_WHILE:{
    if(!swan_check_expr(p,s->condition,symbols,err))return false;
    size_t outer=symbols->count;bool before[1024];
    for(size_t k=0;k<outer;k++)before[k]=symbols->items[k].manual_freed;
    if(!swan_check_block(p,s->body,s->body_count,symbols,err,loop_depth+1)){swan_symbols_leave(symbols,outer);return false;}
    for(size_t k=0;k<outer;k++)if(symbols->items[k].manual_checked&&symbols->items[k].manual_freed!=before[k]){
     swan_error(err,&s->token,"manual variable '%s' cannot change ownership inside a loop; free it after the loop",symbols->items[k].name);swan_symbols_leave(symbols,outer);return false;
    }
    swan_symbols_leave(symbols,outer);
    break;
   }
   case S_BREAK:case S_CONTINUE:
    if(!loop_depth){swan_error(err,&s->token,"break and continue are only valid inside a loop");return false;}
    break;
   case S_PASS:break;
   default:swan_error(err,&s->token,"statement kind %d is not implemented by semantic analysis",(int)s->kind);return false;
  }
 }
 for(size_t k=scope_mark;k<symbols->count;k++)if(symbols->items[k].manual_checked&&!symbols->items[k].manual_freed){
  swan_error(err,NULL,"manual variable '%s' must be freed before leaving its scope",symbols->items[k].name);return false;
 }
 return true;
}
static inline void swan_warning_add(Program *program,const Token *token,const char *format,...){
 if(program->warning_count>=sizeof(program->warnings)/sizeof(program->warnings[0])){
  program->warnings_dropped++;
  return;
 }
 SwanWarning *warning=&program->warnings[program->warning_count++];
 warning->line=token?token->line:0;
 warning->col=token?token->col:0;
 va_list args;
 va_start(args,format);
 vsnprintf(warning->message,sizeof(warning->message),format,args);
 va_end(args);
}
static inline void swan_warning_expr(Program *program,Expr *expression){
 if(!expression)return;
 if(expression->kind==E_BINARY&&expression->token.kind==T_ASSIGN&&
    expression->left&&expression->right&&
    expression->left->kind==E_NAME&&expression->right->kind==E_NAME&&
    expression->left->token.text&&expression->right->token.text&&
    !strcmp(expression->left->token.text,expression->right->token.text)){
  swan_warning_add(program,&expression->token,
      "self-assignment to '%s' has no effect",expression->left->token.text);
 }
 swan_warning_expr(program,expression->left);
 swan_warning_expr(program,expression->right);
 for(size_t i=0;i<expression->argc;i++)swan_warning_expr(program,expression->args[i]);
}
static inline void swan_warning_block(Program *program,Stmt **body,size_t count){
 for(size_t i=0;i<count;i++){
  Stmt *statement=body[i];
  if(!statement)continue;
  swan_warning_expr(program,statement->value);
  swan_warning_expr(program,statement->condition);
  if((statement->kind==S_IF||statement->kind==S_WHILE)&&
     statement->condition&&statement->condition->kind==E_BOOL){
   bool condition=!strcmp(statement->condition->token.text,"true");
   if(statement->kind==S_IF){
    swan_warning_add(program,&statement->token,
      "condition is always %s; one branch is unreachable",condition?"true":"false");
   }else if(!condition){
    swan_warning_add(program,&statement->token,
      "loop condition is always false; loop body will never execute");
   }else{
    swan_warning_add(program,&statement->token,
      "loop condition is always true; verify that the loop can terminate");
   }
  }
  swan_warning_block(program,statement->body,statement->body_count);
  swan_warning_block(program,statement->alternate,statement->alt_count);
 }
}
static inline void swan_collect_warnings(Program *program){
 program->warning_count=0;
 program->warnings_dropped=0;
 for(size_t i=0;i<program->source_function_count;i++){
  Stmt *function=program->functions[i];
  if(!function||function->external)continue;
  swan_warning_block(program,function->body,function->body_count);
  if(program->reachability_computed&&!program->function_reachable[i]&&
     !function->exported&&strcmp(function->name,"main")){
   swan_warning_add(program,&function->token,
       "function '%s' is never reached from main",function->name);
  }
 }
}
static inline size_t swan_print_warnings(const Program *program){
 for(size_t i=0;i<program->warning_count;i++){
  const SwanWarning *warning=&program->warnings[i];
  if(warning->line&&warning->col)
   fprintf(stderr,"swan:%u:%u: warning: %s\n",warning->line,warning->col,warning->message);
  else fprintf(stderr,"swan: warning: %s\n",warning->message);
 }
 if(program->warnings_dropped)
  fprintf(stderr,"swan: warning: %zu additional warning(s) suppressed by diagnostic limit\n",program->warnings_dropped);
 return program->warning_count+program->warnings_dropped;
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
  Stmt *fn=p->functions[i];if(fn->external)continue;symbols.count=0;memset(symbols.buckets,0,sizeof(symbols.buckets));
  for(size_t a=0;a<fn->param_count;a++){
   if(swan_symbol_find(&symbols,fn->params[a],NULL)){swan_error(err,&fn->token,"duplicate parameter '%s'",fn->params[a]);ok=false;break;}
   if(!swan_symbol_add(&symbols,fn->params[a],false,false,false)){swan_error(err,&fn->token,"too many parameters");ok=false;break;}
  }
  if(ok)ok=swan_check_block(p,fn->body,fn->body_count,&symbols,err,0);
 }
 free(symbols.items);return ok;
}
#endif
