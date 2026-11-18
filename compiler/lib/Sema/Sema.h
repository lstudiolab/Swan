#ifndef SWAN_SEMA_H
#define SWAN_SEMA_H
#include "swan/parser.h"
typedef struct { const char *name; bool mutable_value,initialized,is_array,manual_memory,manual_checked,manual_freed,manual_borrow; const char *owner_name; size_t previous; } SwanSymbol;
typedef struct { SwanSymbol *items; size_t count,capacity; size_t *buckets,bucket_count; } SwanSymbols;
static inline unsigned swan_name_hash(const char *s){uint32_t h=2166136261u;for(;*s;s++)h=(h^(unsigned char)*s)*16777619u;return h;}
static inline bool swan_symbols_rehash(SwanSymbols *s,size_t wanted){
 size_t count=16;while(count<wanted){if(count>SIZE_MAX/2)return false;count*=2;}
 if(count>SIZE_MAX/sizeof(*s->buckets))return false;
 size_t *buckets=malloc(count*sizeof(*buckets));
 if(!buckets)return false;
 for(size_t i=0;i<count;i++)buckets[i]=SIZE_MAX;
 for(size_t i=0;i<s->count;i++){size_t b=swan_name_hash(s->items[i].name)&(count-1);s->items[i].previous=buckets[b];buckets[b]=i;}
 free(s->buckets);s->buckets=buckets;s->bucket_count=count;return true;
}
static inline bool swan_symbols_init(SwanSymbols *s){memset(s,0,sizeof(*s));return swan_symbols_rehash(s,16);}
static inline void swan_symbols_destroy(SwanSymbols *s){if(!s)return;free(s->items);free(s->buckets);memset(s,0,sizeof(*s));}
static inline void swan_symbols_reset(SwanSymbols *s){s->count=0;for(size_t i=0;i<s->bucket_count;i++)s->buckets[i]=SIZE_MAX;}
static inline Stmt *swan_find_function(Program *p,const char *name){
 Stmt *local=NULL,*imported=NULL;
 if(p->function_indexed&&p->function_bucket_count){
  size_t b=swan_name_hash(name)&(p->function_bucket_count-1);
  for(size_t n=p->function_buckets[b];n!=SIZE_MAX;n=p->function_next[n]){
   Stmt *fn=p->functions[n];if(strcmp(fn->name,name))continue;
   if(!fn->imported){local=fn;break;}
   if(imported)return NULL;
   imported=fn;
  }
 }else{
  for(size_t i=0;i<p->count;i++){Stmt *fn=p->functions[i];if(strcmp(fn->name,name))continue;if(!fn->imported){local=fn;break;}if(imported)return NULL;imported=fn;}
 }
 return local?local:imported;
}
static inline Stmt *swan_find_link_function(Program *p,const char *link_name){
 if(!p||!link_name)return NULL;
 if(p->function_indexed&&p->function_bucket_count){
  size_t b=swan_name_hash(link_name)&(p->function_bucket_count-1);
  for(size_t n=p->link_function_buckets[b];n!=SIZE_MAX;n=p->link_function_next[n]){
   Stmt *fn=p->functions[n];const char *symbol=fn->link_name?fn->link_name:fn->name;if(!strcmp(symbol,link_name))return fn;
  }
  return NULL;
 }
 for(size_t i=0;i<p->count;i++){Stmt *fn=p->functions[i];const char *symbol=fn->link_name?fn->link_name:fn->name;if(!strcmp(symbol,link_name))return fn;}
 return NULL;
}
static inline Stmt *swan_find_module_function(Program *p,const char *module_name,const char *function_name){
 if(!p||!module_name||!function_name)return NULL;
 for(size_t m=0;m<p->module_count;m++)if(!strcmp(p->module_names[m],module_name))
  for(size_t i=p->module_starts[m];i<p->module_ends[m];i++)if(!strcmp(p->functions[i]->name,function_name))return p->functions[i];
 return NULL;
}
static inline bool swan_function_name_ambiguous(Program *p,const char *name){
 bool found=false;for(size_t i=0;i<p->count;i++){Stmt *fn=p->functions[i];if(fn->imported&&!strcmp(fn->name,name)){if(found)return true;found=true;}}return false;
}
static inline bool swan_symbol_find(SwanSymbols *s,const char *name,size_t *index){
 if(!s->bucket_count)return false;
 size_t b=swan_name_hash(name)&(s->bucket_count-1);
 for(size_t n=s->buckets[b];n!=SIZE_MAX;n=s->items[n].previous){
  if(!strcmp(s->items[n].name,name)){if(index)*index=n;return true;}
 }
 return false;
}
static inline bool swan_symbol_add(SwanSymbols *s,const char *name,bool mut,bool initialized,bool is_array,bool manual,bool checked,bool borrow,const char *owner_name){
 if(s->count==SIZE_MAX||!swan_reserve_array((void**)&s->items,&s->capacity,s->count+1,sizeof(*s->items)))return false;
 if(s->count+1>s->bucket_count-s->bucket_count/4){if(s->bucket_count>SIZE_MAX/2||!swan_symbols_rehash(s,s->bucket_count*2))return false;}
 size_t b=swan_name_hash(name)&(s->bucket_count-1),i=s->count++;SwanSymbol *v=&s->items[i];
 v->name=name;v->mutable_value=mut;v->initialized=initialized;v->is_array=is_array;v->manual_memory=manual;v->manual_checked=checked;v->manual_freed=false;v->manual_borrow=borrow;v->owner_name=owner_name;v->previous=s->buckets[b];s->buckets[b]=i;return true;
}
static inline void swan_symbols_leave(SwanSymbols *s,size_t mark){while(s->count>mark){size_t i=--s->count;size_t b=swan_name_hash(s->items[i].name)&(s->bucket_count-1);s->buckets[b]=s->items[i].previous;}}
static inline bool swan_expr_is_array(Expr *expression,SwanSymbols *symbols){
 if(!expression)return false;
 if(expression->kind==E_CALL){
  const char *callee=swan_expr_terminal_name(expression->left);
  return callee&&(!strcmp(callee,"array")||!strcmp(callee,"array_u32")||
      !strcmp(callee,"array_unchecked")||!strcmp(callee,"array_u32_unchecked"));
 }
 if(expression->kind==E_NAME){
  size_t index;
  return swan_symbol_find(symbols,expression->token.text,&index)&&
         symbols->items[index].is_array;
 }
 return false;
}
static inline bool swan_check_expr(Program *p,Expr *e,SwanSymbols *symbols,SwanError *err){
 if(!e)return true;
 switch(e->kind){
  case E_INT:case E_FLOAT:case E_STRING:case E_BOOL:return true;
  case E_NAME:{size_t index;if(!swan_symbol_find(symbols,e->token.text,&index)){swan_error(err,&e->token,"unknown name '%s'",e->token.text);return false;}SwanSymbol *v=&symbols->items[index];if(!v->initialized){swan_error(err,&e->token,"use of uninitialized variable '%s'",e->token.text);return false;}if(v->manual_checked){if(v->manual_freed){swan_error(err,&e->token,"use of moved or dropped owner '%s'",e->token.text);return false;}swan_error(err,&e->token,"owned values must be accessed through borrow or transferred with move");return false;}return true;}
  case E_CALL:{
   const char *callee=swan_expr_terminal_name(e->left);if(!callee){swan_error(err,&e->token,"call target must be a function name or module-qualified function path");return false;}
   if(e->left&&e->left->kind==E_NAME&&(!strcmp(callee,"array")||!strcmp(callee,"array_u32")||!strcmp(callee,"array_unchecked")||!strcmp(callee,"array_u32_unchecked"))){
    if(e->argc!=1){swan_error(err,&e->token,"array allocation expects exactly one length argument");return false;}
    return swan_check_expr(p,e->args[0],symbols,err);
   }
   Stmt *fn=NULL;
   if(e->left&&e->left->kind==E_MEMBER&&e->left->left&&e->left->left->kind==E_NAME&&e->left->right&&e->left->right->kind==E_NAME){
    const char *module=e->left->left->token.text;
    fn=swan_find_module_function(p,module,callee);
    if(!fn){swan_error(err,&e->token,"module '%s' does not export a function named '%s'",module,callee);return false;}
    Token member=e->left->right->token;member.text=fn->link_name?fn->link_name:fn->name;
    e->left->kind=E_NAME;e->left->token=member;e->left->left=NULL;e->left->right=NULL;
   }else{
    fn=swan_find_function(p,callee);
    if(!fn){if(swan_function_name_ambiguous(p,callee))swan_error(err,&e->token,"function '%s' is ambiguous; qualify it with its module name",callee);else swan_error(err,&e->token,"unknown function '%s'",callee);return false;}
    if(e->left&&e->left->kind==E_NAME){Token callee_token=e->left->token;callee_token.text=fn->link_name?fn->link_name:fn->name;e->left->kind=E_NAME;e->left->token=callee_token;e->left->left=NULL;e->left->right=NULL;}
   }
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
   {size_t base_index;if(swan_symbol_find(symbols,e->left->token.text,&base_index)){
    if(!symbols->items[base_index].is_array){swan_error(err,&e->left->token,"value '%s' is not an array",e->left->token.text);return false;}
    if(!symbols->items[base_index].initialized){swan_error(err,&e->left->token,"use of uninitialized array '%s'",e->left->token.text);return false;}
   }}
   {size_t base_index;if(swan_symbol_find(symbols,e->left->token.text,&base_index)&&symbols->items[base_index].manual_checked&&symbols->items[base_index].manual_freed){swan_error(err,&e->left->token,"use of manually freed variable '%s'",e->left->token.text);return false;}}
   return swan_check_expr(p,e->right,symbols,err);
  case E_UNARY:
   if(e->token.kind==T_BORROW){
    if(!e->left||e->left->kind!=E_NAME){swan_error(err,&e->token,"borrow expects an owned variable name");return false;}
    size_t index;if(!swan_symbol_find(symbols,e->left->token.text,&index)||!symbols->items[index].manual_checked){swan_error(err,&e->token,"borrow requires an owned value");return false;}
    if(symbols->items[index].manual_freed){swan_error(err,&e->token,"cannot borrow a moved or dropped owner");return false;}
    return true;
   }
   if(e->token.kind==T_MOVE){
    if(!e->left||e->left->kind!=E_NAME){swan_error(err,&e->token,"move expects an owned variable name");return false;}
    size_t index;if(!swan_symbol_find(symbols,e->left->token.text,&index)||!symbols->items[index].manual_checked){swan_error(err,&e->token,"move requires an owned value");return false;}
    if(symbols->items[index].manual_freed){swan_error(err,&e->token,"cannot move an owner that was already moved or dropped");return false;}
    for(size_t k=0;k<symbols->count;k++)if(symbols->items[k].manual_borrow&&symbols->items[k].owner_name&&!strcmp(symbols->items[k].owner_name,e->left->token.text)){
     swan_error(err,&e->token,"cannot move '%s' while borrow '%s' is in scope",e->left->token.text,symbols->items[k].name);return false;
    }
    symbols->items[index].manual_freed=true;return true;
   }
   return swan_check_expr(p,e->left,symbols,err);
  case E_BINARY:
   if(e->token.kind==T_ASSIGN){
    if(e->right&&e->right->kind==E_NAME){size_t right_index;if(swan_symbol_find(symbols,e->right->token.text,&right_index)&&symbols->items[right_index].manual_checked){swan_error(err,&e->right->token,"manual ownership cannot be copied or aliased");return false;}}
    if(e->left&&e->left->kind==E_INDEX){
     if(!e->left->left||e->left->left->kind!=E_NAME||!swan_symbol_find(symbols,e->left->left->token.text,NULL)){
      swan_error(err,&e->token,"array assignment requires a declared array variable");return false;
     }
     {size_t base_index;if(swan_symbol_find(symbols,e->left->left->token.text,&base_index)){
      if(!symbols->items[base_index].is_array){swan_error(err,&e->left->left->token,"value '%s' is not an array",e->left->left->token.text);return false;}
      if(!symbols->items[base_index].initialized){swan_error(err,&e->left->left->token,"use of uninitialized array '%s'",e->left->left->token.text);return false;}
     }}
     {size_t base_index;if(swan_symbol_find(symbols,e->left->left->token.text,&base_index)&&symbols->items[base_index].manual_checked&&symbols->items[base_index].manual_freed){swan_error(err,&e->left->left->token,"use of manually freed variable '%s'",e->left->left->token.text);return false;}}
     return swan_check_expr(p,e->left->right,symbols,err)&&swan_check_expr(p,e->right,symbols,err);
    }
    if(!e->left||e->left->kind!=E_NAME){swan_error(err,&e->token,"assignment target must be a variable or array element");return false;}
    size_t index;if(!swan_symbol_find(symbols,e->left->token.text,&index)){swan_error(err,&e->left->token,"unknown variable '%s'",e->left->token.text);return false;}
    if(symbols->items[index].manual_checked){swan_error(err,&e->left->token,"manual variables cannot be reassigned or moved");return false;}
    if(!symbols->items[index].mutable_value){swan_error(err,&e->left->token,"cannot assign to immutable value '%s'",e->left->token.text);return false;}
    if(!swan_check_expr(p,e->right,symbols,err))return false;
    bool right_is_array=swan_expr_is_array(e->right,symbols);
    if(symbols->items[index].initialized&&
       symbols->items[index].is_array!=right_is_array){
     swan_error(err,&e->token,"assignment changes the value between array and scalar types");
     return false;
    }
    symbols->items[index].is_array=right_is_array;
    symbols->items[index].initialized=true;
    return true;
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
    if(swan_symbol_find(symbols,s->name,NULL)){swan_error(err,&s->token,"'%s' is already declared",s->name);return false;}
    const char *owner_name=NULL;
    if(s->manual_checked){
     if(!p->raw_mode){swan_error(err,&s->token,"own declarations require 'import raw'");return false;}
     if(!s->mutable_value||!s->value){swan_error(err,&s->token,"own must declare an initialized mutable variable");return false;}
     if(s->type&&strcmp(s->type,"i64")){swan_error(err,&s->token,"raw owners currently require the i64 address type");return false;}
     if(!swan_check_expr(p,s->value,symbols,err))return false;
    }else if(s->manual_borrow){
     if(!p->raw_mode){swan_error(err,&s->token,"borrow declarations require 'import raw'");return false;}
     if(s->mutable_value||!s->value||s->value->kind!=E_NAME){swan_error(err,&s->token,"borrow must declare an immutable view of an owned variable");return false;}
     size_t owner;if(!swan_symbol_find(symbols,s->value->token.text,&owner)||!symbols->items[owner].manual_checked){swan_error(err,&s->value->token,"borrow source must be an owned variable");return false;}
     if(symbols->items[owner].manual_freed){swan_error(err,&s->value->token,"cannot borrow a moved or dropped owner");return false;}
     owner_name=s->value->token.text;
    }else{
     if(s->value&&s->value->kind==E_NAME){size_t source;if(swan_symbol_find(symbols,s->value->token.text,&source)&&symbols->items[source].manual_checked){swan_error(err,&s->value->token,"owned values cannot be copied; use borrow or move");return false;}}
     if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;
    }
    bool is_array=(s->type&&!strcmp(s->type,"u32_array"))||swan_expr_is_array(s->value,symbols);
    if(!swan_symbol_add(symbols,s->name,s->mutable_value,s->value!=NULL,is_array,s->manual_memory,s->manual_checked,s->manual_borrow,owner_name)){swan_error(err,&s->token,"too many local variables");return false;}
    break;
   }
   case S_LOG:
    if(s->value&&s->value->kind==E_NAME){size_t index;if(swan_symbol_find(symbols,s->value->token.text,&index)&&symbols->items[index].manual_checked){swan_error(err,&s->value->token,"manual allocation handles cannot be printed or copied");return false;}}
    if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;
    break;
   case S_RETURN:
    if(s->value&&s->value->kind==E_NAME){size_t index;if(swan_symbol_find(symbols,s->value->token.text,&index)&&(symbols->items[index].manual_memory||symbols->items[index].manual_borrow)){swan_error(err,&s->value->token,"raw owners and borrows cannot escape through a return value");return false;}}
    if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;
    for(size_t k=0;k<symbols->count;k++)if(symbols->items[k].manual_checked&&!symbols->items[k].manual_freed){swan_error(err,&s->token,"manual variable '%s' must be freed before returning",symbols->items[k].name);return false;}
    break;
   case S_DROP:{
    if(!p->raw_mode){swan_error(err,&s->token,"drop requires 'import raw'");return false;}
    if(!s->value||s->value->kind!=E_NAME){swan_error(err,&s->token,"drop expects an owned variable");return false;}
    size_t owner;if(!swan_symbol_find(symbols,s->value->token.text,&owner)||!symbols->items[owner].manual_checked){swan_error(err,&s->value->token,"drop requires an owned variable");return false;}
    if(symbols->items[owner].manual_freed){swan_error(err,&s->value->token,"owner '%s' was already moved or dropped",s->value->token.text);return false;}
    for(size_t k=0;k<symbols->count;k++)if(symbols->items[k].manual_borrow&&symbols->items[k].owner_name&&!strcmp(symbols->items[k].owner_name,s->value->token.text)){
     swan_error(err,&s->value->token,"cannot drop '%s' while borrow '%s' is in scope",s->value->token.text,symbols->items[k].name);return false;
    }
    symbols->items[owner].manual_freed=true;break;
   }
   case S_EXPR:
    if(s->value&&s->value->kind==E_UNARY&&s->value->token.kind==T_MOVE){swan_error(err,&s->value->token,"move must transfer into an owner or an ownership-taking call");return false;}
    if(s->value&&!swan_check_expr(p,s->value,symbols,err))return false;
    break;
   case S_IF:{
    if(!swan_check_expr(p,s->condition,symbols,err))return false;
    size_t outer=symbols->count;if(outer>SIZE_MAX/(6*sizeof(bool))){swan_error(err,&s->token,"branch state size overflow");return false;}
    bool *states=calloc(outer?outer*6:1,sizeof(*states));if(!states){swan_error(err,&s->token,"out of memory tracking branch state");return false;}
    bool *before=states,*then_state=states+outer,*else_state=states+outer*2,*before_initialized=states+outer*3,*then_initialized=states+outer*4,*else_initialized=states+outer*5;
    for(size_t k=0;k<outer;k++){before[k]=symbols->items[k].manual_freed;before_initialized[k]=symbols->items[k].initialized;}
    if(!swan_check_block(p,s->body,s->body_count,symbols,err,loop_depth)){swan_symbols_leave(symbols,outer);free(states);return false;}
    for(size_t k=0;k<outer;k++){then_state[k]=symbols->items[k].manual_freed;then_initialized[k]=symbols->items[k].initialized;}
    swan_symbols_leave(symbols,outer);for(size_t k=0;k<outer;k++){symbols->items[k].manual_freed=before[k];symbols->items[k].initialized=before_initialized[k];}
    if(!swan_check_block(p,s->alternate,s->alt_count,symbols,err,loop_depth)){swan_symbols_leave(symbols,outer);free(states);return false;}
    for(size_t k=0;k<outer;k++){else_state[k]=symbols->items[k].manual_freed;else_initialized[k]=symbols->items[k].initialized;}
    swan_symbols_leave(symbols,outer);
    for(size_t k=0;k<outer;k++){if(symbols->items[k].manual_checked&&then_state[k]!=else_state[k]){swan_error(err,&s->token,"manual variable '%s' must have the same ownership state on every branch",symbols->items[k].name);free(states);return false;}symbols->items[k].manual_freed=then_state[k];symbols->items[k].initialized=then_initialized[k]&&else_initialized[k];}
    free(states);break;
   }
   case S_WHILE:{
    if(!swan_check_expr(p,s->condition,symbols,err))return false;
    size_t outer=symbols->count;if(outer>SIZE_MAX/(2*sizeof(bool))){swan_error(err,&s->token,"loop state size overflow");return false;}
    bool *states=calloc(outer?outer*2:1,sizeof(*states));if(!states){swan_error(err,&s->token,"out of memory tracking loop state");return false;}
    bool *before=states,*before_initialized=states+outer;for(size_t k=0;k<outer;k++){before[k]=symbols->items[k].manual_freed;before_initialized[k]=symbols->items[k].initialized;}
    if(!swan_check_block(p,s->body,s->body_count,symbols,err,loop_depth+1)){swan_symbols_leave(symbols,outer);free(states);return false;}
    for(size_t k=0;k<outer;k++)if(symbols->items[k].manual_checked&&symbols->items[k].manual_freed!=before[k]){swan_error(err,&s->token,"manual variable '%s' cannot change ownership inside a loop; free it after the loop",symbols->items[k].name);swan_symbols_leave(symbols,outer);free(states);return false;}
    swan_symbols_leave(symbols,outer);for(size_t k=0;k<outer;k++)symbols->items[k].initialized=before_initialized[k];free(states);break;
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
 if(program->warning_count==SIZE_MAX||!swan_reserve_array((void**)&program->warnings,&program->warning_capacity,program->warning_count+1,sizeof(*program->warnings))){
  program->warnings_dropped++;return;
 }
 SwanWarning *warning=&program->warnings[program->warning_count++];
 warning->line=token?token->line:0;
 warning->col=token?token->col:0;
 warning->source_name=token?token->source_name:NULL;
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
 if(expression->kind==E_BINARY&&
    (expression->token.kind==T_SLASH||expression->token.kind==T_FLOORDIV||
     expression->token.kind==T_PERCENT)&&
    expression->right&&expression->right->kind==E_INT&&
    expression->right->token.text&&!strcmp(expression->right->token.text,"0")){
  swan_warning_add(program,&expression->token,
      "division or remainder by the literal zero will fail at runtime");
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
     statement->condition&&statement->condition->kind==E_BOOL&&
     statement->condition->token.text){
   bool condition=!strcmp(statement->condition->token.text,"true");
   if(statement->kind==S_IF){
    swan_warning_add(program,&statement->token,
      "condition is always %s; one branch is unreachable",condition?"true":"false");
   }else if(!condition){
    swan_warning_add(program,&statement->token,
      "loop condition is always false; loop body will never execute");
   }else if(statement->token.kind!=T_LOOP){
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
  if(warning->source_name&&warning->line&&warning->col)
   fprintf(stderr,"swan:%s:%u:%u: warning: %s\n",warning->source_name,warning->line,warning->col,warning->message);
  else if(warning->line&&warning->col)
   fprintf(stderr,"swan:%u:%u: warning: %s\n",warning->line,warning->col,warning->message);
  else fprintf(stderr,"swan: warning: %s\n",warning->message);
 }
 if(program->warnings_dropped)
  fprintf(stderr,"swan: warning: %zu additional warning(s) suppressed by diagnostic limit\n",program->warnings_dropped);
 return program->warning_count+program->warnings_dropped;
}

static inline bool swan_validate_local_names(Stmt **body,size_t count,SwanSymbols *names,SwanError *error){
 for(size_t i=0;i<count;i++){Stmt *statement=body[i];if(!statement)continue;
  if(statement->kind==S_VARIABLE){
   if(swan_symbol_find(names,statement->name,NULL)){swan_error(error,&statement->token,"local variable '%s' is redeclared; local shadowing is not supported",statement->name);return false;}
   if(!swan_symbol_add(names,statement->name,false,true,false,false,false,false,NULL)){swan_error(error,&statement->token,"out of memory validating local declarations");return false;}
  }
  if(!swan_validate_local_names(statement->body,statement->body_count,names,error)||!swan_validate_local_names(statement->alternate,statement->alt_count,names,error))return false;
 }return true;
}

static inline bool swan_semantic_check(Program *p,SwanError *err){
 if(err)memset(err,0,sizeof(*err));
 if(!p){swan_error(err,NULL,"semantic analysis requires a program");return false;}
 for(size_t i=0;i<p->count;i++){Stmt *fn=p->functions[i];if(!fn||!fn->name||!fn->name[0]){swan_error(err,fn?&fn->token:NULL,"program contains an invalid function declaration");return false;}if(fn->param_count&&!fn->params){swan_error(err,&fn->token,"function '%s' has invalid parameter storage",fn->name);return false;}}
 if(p->count>SIZE_MAX/2){swan_error(err,NULL,"function index size overflow");return false;}size_t bucket_count=16;
 while(bucket_count<p->count*2){if(bucket_count>SIZE_MAX/2){swan_error(err,NULL,"function index size overflow");return false;}bucket_count*=2;}
 if(bucket_count>SIZE_MAX/sizeof(*p->function_buckets)||p->count>SIZE_MAX/sizeof(*p->function_next)){swan_error(err,NULL,"function index size overflow");return false;}
 size_t *buckets=malloc(bucket_count*sizeof(*buckets));
 size_t *next=p->count?malloc(p->count*sizeof(*next)):NULL;
 size_t *link_buckets=malloc(bucket_count*sizeof(*link_buckets));
 size_t *link_next=p->count?malloc(p->count*sizeof(*link_next)):NULL;
 if(!buckets||!link_buckets||(p->count&&(!next||!link_next))){
  free(buckets);free(next);free(link_buckets);free(link_next);
  swan_error(err,NULL,"out of memory building function indexes");return false;
 }
 for(size_t i=0;i<bucket_count;i++){buckets[i]=SIZE_MAX;link_buckets[i]=SIZE_MAX;}
 for(size_t i=0;i<p->count;i++){
  Stmt *current=p->functions[i];size_t b=swan_name_hash(current->name)&(bucket_count-1);
  for(size_t n=buckets[b];n!=SIZE_MAX;n=next[n]){
   Stmt *prior=p->functions[n];
   if(!strcmp(prior->name,current->name)&&
      ((!prior->imported&&!current->imported)||
       (prior->imported&&current->imported&&prior->module_index==current->module_index))){
    swan_error(err,&current->token,"function '%s' is already declared in this scope",current->name);
    free(buckets);free(next);free(link_buckets);free(link_next);return false;
   }
  }
  next[i]=buckets[b];buckets[b]=i;
  const char *link_name=current->link_name?current->link_name:current->name;
  size_t lb=swan_name_hash(link_name)&(bucket_count-1);
  for(size_t n=link_buckets[lb];n!=SIZE_MAX;n=link_next[n])
   if(!strcmp(p->functions[n]->link_name?p->functions[n]->link_name:p->functions[n]->name,link_name)){
    swan_error(err,&current->token,"internal linker symbol '%s' is duplicated",link_name);
    free(buckets);free(next);free(link_buckets);free(link_next);return false;
   }
  link_next[i]=link_buckets[lb];link_buckets[lb]=i;
 }
 free(p->function_buckets);free(p->function_next);free(p->link_function_buckets);free(p->link_function_next);
 p->function_buckets=buckets;p->function_next=next;
 p->link_function_buckets=link_buckets;p->link_function_next=link_next;
 p->function_bucket_count=bucket_count;p->function_indexed=true;
 if(!swan_find_function(p,"main")){swan_error(err,NULL,"program must define fn main");return false;}
 SwanSymbols symbols={0};if(!swan_symbols_init(&symbols)){swan_error(err,NULL,"out of memory initializing semantic symbol table");return false;}bool ok=true;
 for(size_t i=0;i<p->count&&ok;i++){
  Stmt *fn=p->functions[i];if(fn->external)continue;
  swan_symbols_reset(&symbols);
  SwanSymbols local_names={0};
  if(!swan_symbols_init(&local_names)){swan_error(err,&fn->token,"out of memory initializing local declaration index");ok=false;break;}
  for(size_t a=0;a<fn->param_count;a++){
   if(!fn->params[a]||!fn->params[a][0]){swan_error(err,&fn->token,"function '%s' has an invalid parameter name",fn->name);ok=false;break;}
   if(swan_symbol_find(&symbols,fn->params[a],NULL)){swan_error(err,&fn->token,"duplicate parameter '%s'",fn->params[a]);ok=false;break;}
   bool is_array=fn->param_types&&fn->param_types[a]&&!strcmp(fn->param_types[a],"u32_array");
   if(!swan_symbol_add(&symbols,fn->params[a],false,true,is_array,false,false,false,NULL)||
      !swan_symbol_add(&local_names,fn->params[a],false,true,is_array,false,false,false,NULL)){
    swan_error(err,&fn->token,"out of memory adding function parameter");ok=false;break;
   }
  }
  if(ok)ok=swan_validate_local_names(fn->body,fn->body_count,&local_names,err);
  swan_symbols_destroy(&local_names);
  if(ok)ok=swan_check_block(p,fn->body,fn->body_count,&symbols,err,0);
 }
 swan_symbols_destroy(&symbols);return ok;
}
#endif
