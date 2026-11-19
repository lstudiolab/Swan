#ifndef BREW_SEMA_H
#define BREW_SEMA_H
#include "brew/parser.h"
typedef struct { const char *name; bool mutable_value,initialized,is_array; size_t previous; } BrewSymbol;
typedef struct { BrewSymbol *items; size_t count,capacity; size_t *buckets,bucket_count; } BrewSymbols;
static inline unsigned brew_name_hash(const char *s){uint32_t h=2166136261u;for(;*s;s++)h=(h^(unsigned char)*s)*16777619u;return h;}
static inline bool brew_symbols_rehash(BrewSymbols *s,size_t wanted){
 size_t count=16;while(count<wanted){if(count>SIZE_MAX/2)return false;count*=2;}
 if(count>SIZE_MAX/sizeof(*s->buckets))return false;
 size_t *buckets=malloc(count*sizeof(*buckets));
 if(!buckets)return false;
 for(size_t i=0;i<count;i++)buckets[i]=SIZE_MAX;
 for(size_t i=0;i<s->count;i++){size_t b=brew_name_hash(s->items[i].name)&(count-1);s->items[i].previous=buckets[b];buckets[b]=i;}
 free(s->buckets);s->buckets=buckets;s->bucket_count=count;return true;
}
static inline bool brew_symbols_init(BrewSymbols *s){memset(s,0,sizeof(*s));return brew_symbols_rehash(s,16);}
static inline void brew_symbols_destroy(BrewSymbols *s){if(!s)return;free(s->items);free(s->buckets);memset(s,0,sizeof(*s));}
static inline void brew_symbols_reset(BrewSymbols *s){s->count=0;for(size_t i=0;i<s->bucket_count;i++)s->buckets[i]=SIZE_MAX;}
static inline Stmt *brew_find_function(Program *p,const char *name){
 Stmt *local=NULL,*imported=NULL;
 if(p->function_indexed&&p->function_bucket_count){
  size_t b=brew_name_hash(name)&(p->function_bucket_count-1);
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
static inline Stmt *brew_find_link_function(Program *p,const char *link_name){
 if(!p||!link_name)return NULL;
 if(p->function_indexed&&p->function_bucket_count){
  size_t b=brew_name_hash(link_name)&(p->function_bucket_count-1);
  for(size_t n=p->link_function_buckets[b];n!=SIZE_MAX;n=p->link_function_next[n]){
   Stmt *fn=p->functions[n];const char *symbol=fn->link_name?fn->link_name:fn->name;if(!strcmp(symbol,link_name))return fn;
  }
  return NULL;
 }
 for(size_t i=0;i<p->count;i++){Stmt *fn=p->functions[i];const char *symbol=fn->link_name?fn->link_name:fn->name;if(!strcmp(symbol,link_name))return fn;}
 return NULL;
}
static inline Stmt *brew_find_module_function(Program *p,const char *module_name,const char *function_name){
 if(!p||!module_name||!function_name)return NULL;
 for(size_t m=0;m<p->module_count;m++)if(!strcmp(p->module_names[m],module_name))
  for(size_t i=p->module_starts[m];i<p->module_ends[m];i++)if(!strcmp(p->functions[i]->name,function_name))return p->functions[i];
 return NULL;
}
static inline bool brew_function_name_ambiguous(Program *p,const char *name){
 bool found=false;for(size_t i=0;i<p->count;i++){Stmt *fn=p->functions[i];if(fn->imported&&!strcmp(fn->name,name)){if(found)return true;found=true;}}return false;
}
static inline bool brew_symbol_find(BrewSymbols *s,const char *name,size_t *index){
 if(!s->bucket_count)return false;
 size_t b=brew_name_hash(name)&(s->bucket_count-1);
 for(size_t n=s->buckets[b];n!=SIZE_MAX;n=s->items[n].previous){
  if(!strcmp(s->items[n].name,name)){if(index)*index=n;return true;}
 }
 return false;
}
static inline bool brew_symbol_add(BrewSymbols *s,const char *name,bool mut,bool initialized,bool is_array){
 if(s->count==SIZE_MAX||!brew_reserve_array((void**)&s->items,&s->capacity,s->count+1,sizeof(*s->items)))return false;
 if(s->count+1>s->bucket_count-s->bucket_count/4){if(s->bucket_count>SIZE_MAX/2||!brew_symbols_rehash(s,s->bucket_count*2))return false;}
 size_t b=brew_name_hash(name)&(s->bucket_count-1),i=s->count++;BrewSymbol *v=&s->items[i];
 v->name=name;v->mutable_value=mut;v->initialized=initialized;v->is_array=is_array;v->previous=s->buckets[b];s->buckets[b]=i;return true;
}
static inline void brew_symbols_leave(BrewSymbols *s,size_t mark){while(s->count>mark){size_t i=--s->count;size_t b=brew_name_hash(s->items[i].name)&(s->bucket_count-1);s->buckets[b]=s->items[i].previous;}}
static inline bool brew_expr_is_array(Expr *expression,BrewSymbols *symbols){
 if(!expression)return false;
 if(expression->kind==E_CALL){
  const char *callee=brew_expr_terminal_name(expression->left);
  return callee&&(!strcmp(callee,"array")||!strcmp(callee,"array_u32")||
      !strcmp(callee,"array_unchecked")||!strcmp(callee,"array_u32_unchecked"));
 }
 if(expression->kind==E_NAME){
  size_t index;
  return brew_symbol_find(symbols,expression->token.text,&index)&&
         symbols->items[index].is_array;
 }
 return false;
}
static inline bool brew_check_expr(Program *p,Expr *e,BrewSymbols *symbols,BrewError *err){
 if(!e)return true;
 switch(e->kind){
  case E_INT:case E_FLOAT:case E_STRING:case E_BOOL:return true;
  case E_NAME:{size_t index;if(!brew_symbol_find(symbols,e->token.text,&index)){brew_error(err,&e->token,"unknown name '%s'",e->token.text);return false;}BrewSymbol *v=&symbols->items[index];if(!v->initialized){brew_error(err,&e->token,"use of uninitialized variable '%s'",e->token.text);return false;}return true;}
  case E_CALL:{
   const char *callee=brew_expr_terminal_name(e->left);if(!callee){brew_error(err,&e->token,"call target must be a function name or module-qualified function path");return false;}
   if(e->left&&e->left->kind==E_NAME&&(!strcmp(callee,"array")||!strcmp(callee,"array_u32")||!strcmp(callee,"array_unchecked")||!strcmp(callee,"array_u32_unchecked"))){
    if(e->argc!=1){brew_error(err,&e->token,"array allocation expects exactly one length argument");return false;}
    return brew_check_expr(p,e->args[0],symbols,err);
   }
   Stmt *fn=NULL;
   if(e->left&&e->left->kind==E_MEMBER&&e->left->left&&e->left->left->kind==E_NAME&&e->left->right&&e->left->right->kind==E_NAME){
    const char *module=e->left->left->token.text;
    fn=brew_find_module_function(p,module,callee);
    if(!fn){brew_error(err,&e->token,"module '%s' does not export a function named '%s'",module,callee);return false;}
    Token member=e->left->right->token;member.text=fn->link_name?fn->link_name:fn->name;
    e->left->kind=E_NAME;e->left->token=member;e->left->left=NULL;e->left->right=NULL;
   }else{
    fn=brew_find_function(p,callee);
    if(!fn){if(brew_function_name_ambiguous(p,callee))brew_error(err,&e->token,"function '%s' is ambiguous; qualify it with its module name",callee);else brew_error(err,&e->token,"unknown function '%s'",callee);return false;}
    if(e->left&&e->left->kind==E_NAME){Token callee_token=e->left->token;callee_token.text=fn->link_name?fn->link_name:fn->name;e->left->kind=E_NAME;e->left->token=callee_token;e->left->left=NULL;e->left->right=NULL;}
   }
   if(fn->param_count!=e->argc){brew_error(err,&e->token,"function '%s' expects %zu arguments, got %zu",fn->name,fn->param_count,e->argc);return false;}
   for(size_t i=0;i<e->argc;i++){
    if(!brew_check_expr(p,e->args[i],symbols,err))return false;
   }
   return true;
  }
  case E_MEMBER:brew_error(err,&e->token,"member values require struct or module value resolution, which is not implemented yet");return false;
  case E_INDEX:
   if(!e->left||e->left->kind!=E_NAME||!brew_symbol_find(symbols,e->left->token.text,NULL)){
    brew_error(err,&e->token,"array indexing requires a declared array variable");return false;
   }
   {size_t base_index;if(brew_symbol_find(symbols,e->left->token.text,&base_index)){
    if(!symbols->items[base_index].is_array){brew_error(err,&e->left->token,"value '%s' is not an array",e->left->token.text);return false;}
    if(!symbols->items[base_index].initialized){brew_error(err,&e->left->token,"use of uninitialized array '%s'",e->left->token.text);return false;}
   }}
   return brew_check_expr(p,e->right,symbols,err);
  case E_UNARY:return brew_check_expr(p,e->left,symbols,err);
  case E_BINARY:
   if(e->token.kind==T_ASSIGN){
    if(e->left&&e->left->kind==E_INDEX){
     if(!e->left->left||e->left->left->kind!=E_NAME||!brew_symbol_find(symbols,e->left->left->token.text,NULL)){
      brew_error(err,&e->token,"array assignment requires a declared array variable");return false;
     }
     {size_t base_index;if(brew_symbol_find(symbols,e->left->left->token.text,&base_index)){
      if(!symbols->items[base_index].is_array){brew_error(err,&e->left->left->token,"value '%s' is not an array",e->left->left->token.text);return false;}
      if(!symbols->items[base_index].initialized){brew_error(err,&e->left->left->token,"use of uninitialized array '%s'",e->left->left->token.text);return false;}
     }}
     return brew_check_expr(p,e->left->right,symbols,err)&&brew_check_expr(p,e->right,symbols,err);
    }
    if(!e->left||e->left->kind!=E_NAME){brew_error(err,&e->token,"assignment target must be a variable or array element");return false;}
    size_t index;if(!brew_symbol_find(symbols,e->left->token.text,&index)){brew_error(err,&e->left->token,"unknown variable '%s'",e->left->token.text);return false;}
    if(!symbols->items[index].mutable_value){brew_error(err,&e->left->token,"cannot assign to immutable value '%s'",e->left->token.text);return false;}
    if(!brew_check_expr(p,e->right,symbols,err))return false;
    bool right_is_array=brew_expr_is_array(e->right,symbols);
    if(symbols->items[index].initialized&&
       symbols->items[index].is_array!=right_is_array){
     brew_error(err,&e->token,"assignment changes the value between array and scalar types");
     return false;
    }
    symbols->items[index].is_array=right_is_array;
    symbols->items[index].initialized=true;
    return true;
   }
   return brew_check_expr(p,e->left,symbols,err)&&brew_check_expr(p,e->right,symbols,err);
  default:brew_error(err,&e->token,"expression kind %d is not implemented by semantic analysis",(int)e->kind);return false;
 }
}
static inline bool brew_check_block(Program *p,Stmt **body,size_t count,BrewSymbols *symbols,BrewError *err,unsigned loop_depth){
 for(size_t i=0;i<count;i++){
  Stmt *s=body[i];
  switch(s->kind){
   case S_VARIABLE:{
    if(brew_symbol_find(symbols,s->name,NULL)){brew_error(err,&s->token,"'%s' is already declared",s->name);return false;}
    if(s->value&&!brew_check_expr(p,s->value,symbols,err))return false;
    bool is_array=(s->type&&!strcmp(s->type,"u32_array"))||
                  brew_expr_is_array(s->value,symbols);
    if(!brew_symbol_add(symbols,s->name,s->mutable_value,s->value!=NULL,is_array)){brew_error(err,&s->token,"too many local variables");return false;}
    break;
   }
   case S_LOG:
    if(s->value&&!brew_check_expr(p,s->value,symbols,err))return false;
    break;
   case S_RETURN:
    if(s->value&&!brew_check_expr(p,s->value,symbols,err))return false;
    break;
   case S_EXPR:
    if(s->value&&!brew_check_expr(p,s->value,symbols,err))return false;
    break;
   case S_IF:{
    if(!brew_check_expr(p,s->condition,symbols,err))return false;
    size_t outer=symbols->count;
    if(outer>SIZE_MAX/(3*sizeof(bool))){brew_error(err,&s->token,"branch state size overflow");return false;}
    bool *states=calloc(outer?outer*3:1,sizeof(*states));
    if(!states){brew_error(err,&s->token,"out of memory tracking branch state");return false;}
    bool *before=states,*then_initialized=states+outer,*else_initialized=states+outer*2;
    for(size_t k=0;k<outer;k++)before[k]=symbols->items[k].initialized;
    if(!brew_check_block(p,s->body,s->body_count,symbols,err,loop_depth)){brew_symbols_leave(symbols,outer);free(states);return false;}
    for(size_t k=0;k<outer;k++)then_initialized[k]=symbols->items[k].initialized;
    brew_symbols_leave(symbols,outer);
    for(size_t k=0;k<outer;k++)symbols->items[k].initialized=before[k];
    if(!brew_check_block(p,s->alternate,s->alt_count,symbols,err,loop_depth)){brew_symbols_leave(symbols,outer);free(states);return false;}
    for(size_t k=0;k<outer;k++)else_initialized[k]=symbols->items[k].initialized;
    brew_symbols_leave(symbols,outer);
    for(size_t k=0;k<outer;k++)symbols->items[k].initialized=then_initialized[k]&&else_initialized[k];
    free(states);break;
   }
   case S_WHILE:{
    if(!brew_check_expr(p,s->condition,symbols,err))return false;
    size_t outer=symbols->count;
    bool *before=calloc(outer?outer:1,sizeof(*before));
    if(!before){brew_error(err,&s->token,"out of memory tracking loop state");return false;}
    for(size_t k=0;k<outer;k++)before[k]=symbols->items[k].initialized;
    if(!brew_check_block(p,s->body,s->body_count,symbols,err,loop_depth+1)){brew_symbols_leave(symbols,outer);free(before);return false;}
    brew_symbols_leave(symbols,outer);
    for(size_t k=0;k<outer;k++)symbols->items[k].initialized=before[k];
    free(before);break;
   }
   case S_BREAK:case S_CONTINUE:
    if(!loop_depth){brew_error(err,&s->token,"break and continue are only valid inside a loop");return false;}
    break;
   case S_PASS:break;
   default:brew_error(err,&s->token,"statement kind %d is not implemented by semantic analysis",(int)s->kind);return false;
  }
 }
 return true;
}
static inline void brew_warning_add(Program *program,const Token *token,const char *format,...){
 if(program->warning_count==SIZE_MAX||!brew_reserve_array((void**)&program->warnings,&program->warning_capacity,program->warning_count+1,sizeof(*program->warnings))){
  program->warnings_dropped++;return;
 }
 BrewWarning *warning=&program->warnings[program->warning_count++];
 warning->line=token?token->line:0;
 warning->col=token?token->col:0;
 warning->source_name=token?token->source_name:NULL;
 va_list args;
 va_start(args,format);
 vsnprintf(warning->message,sizeof(warning->message),format,args);
 va_end(args);
}
static inline void brew_warning_expr(Program *program,Expr *expression){
 if(!expression)return;
 if(expression->kind==E_BINARY&&expression->token.kind==T_ASSIGN&&
    expression->left&&expression->right&&
    expression->left->kind==E_NAME&&expression->right->kind==E_NAME&&
    expression->left->token.text&&expression->right->token.text&&
    !strcmp(expression->left->token.text,expression->right->token.text)){
  brew_warning_add(program,&expression->token,
      "self-assignment to '%s' has no effect",expression->left->token.text);
 }
 if(expression->kind==E_BINARY&&
    (expression->token.kind==T_SLASH||expression->token.kind==T_FLOORDIV||
     expression->token.kind==T_PERCENT)&&
    expression->right&&expression->right->kind==E_INT&&
    expression->right->token.text&&!strcmp(expression->right->token.text,"0")){
  brew_warning_add(program,&expression->token,
      "division or remainder by the literal zero will fail at runtime");
 }
 brew_warning_expr(program,expression->left);
 brew_warning_expr(program,expression->right);
 for(size_t i=0;i<expression->argc;i++)brew_warning_expr(program,expression->args[i]);
}
static inline void brew_warning_block(Program *program,Stmt **body,size_t count){
 for(size_t i=0;i<count;i++){
  Stmt *statement=body[i];
  if(!statement)continue;
  brew_warning_expr(program,statement->value);
  brew_warning_expr(program,statement->condition);
  if((statement->kind==S_IF||statement->kind==S_WHILE)&&
     statement->condition&&statement->condition->kind==E_BOOL&&
     statement->condition->token.text){
   bool condition=!strcmp(statement->condition->token.text,"true");
   if(statement->kind==S_IF){
    brew_warning_add(program,&statement->token,
      "condition is always %s; one branch is unreachable",condition?"true":"false");
   }else if(!condition){
    brew_warning_add(program,&statement->token,
      "loop condition is always false; loop body will never execute");
   }else if(statement->token.kind!=T_LOOP){
    brew_warning_add(program,&statement->token,
      "loop condition is always true; verify that the loop can terminate");
   }
  }
  brew_warning_block(program,statement->body,statement->body_count);
  brew_warning_block(program,statement->alternate,statement->alt_count);
 }
}
static inline void brew_collect_warnings(Program *program){
 program->warning_count=0;
 program->warnings_dropped=0;
 for(size_t i=0;i<program->source_function_count;i++){
  Stmt *function=program->functions[i];
  if(!function||function->external)continue;
  brew_warning_block(program,function->body,function->body_count);
  if(program->reachability_computed&&!program->function_reachable[i]&&
     !function->exported&&strcmp(function->name,"main")){
   brew_warning_add(program,&function->token,
       "function '%s' is never reached from main",function->name);
  }
 }
}
static inline size_t brew_print_warnings(const Program *program){
 for(size_t i=0;i<program->warning_count;i++){
  const BrewWarning *warning=&program->warnings[i];
  if(warning->source_name&&warning->line&&warning->col)
   fprintf(stderr,"brew:%s:%u:%u: warning: %s\n",warning->source_name,warning->line,warning->col,warning->message);
  else if(warning->line&&warning->col)
   fprintf(stderr,"brew:%u:%u: warning: %s\n",warning->line,warning->col,warning->message);
  else fprintf(stderr,"brew: warning: %s\n",warning->message);
 }
 if(program->warnings_dropped)
  fprintf(stderr,"brew: warning: %zu additional warning(s) suppressed by diagnostic limit\n",program->warnings_dropped);
 return program->warning_count+program->warnings_dropped;
}

static inline bool brew_validate_local_names(Stmt **body,size_t count,BrewSymbols *names,BrewError *error){
 for(size_t i=0;i<count;i++){Stmt *statement=body[i];if(!statement)continue;
  if(statement->kind==S_VARIABLE){
   if(brew_symbol_find(names,statement->name,NULL)){brew_error(error,&statement->token,"local variable '%s' is redeclared; local shadowing is not supported",statement->name);return false;}
   if(!brew_symbol_add(names,statement->name,false,true,false)){brew_error(error,&statement->token,"out of memory validating local declarations");return false;}
  }
  if(!brew_validate_local_names(statement->body,statement->body_count,names,error)||!brew_validate_local_names(statement->alternate,statement->alt_count,names,error))return false;
 }return true;
}

static inline bool brew_semantic_check(Program *p,BrewError *err){
 if(err)memset(err,0,sizeof(*err));
 if(!p){brew_error(err,NULL,"semantic analysis requires a program");return false;}
 for(size_t i=0;i<p->count;i++){Stmt *fn=p->functions[i];if(!fn||!fn->name||!fn->name[0]){brew_error(err,fn?&fn->token:NULL,"program contains an invalid function declaration");return false;}if(fn->param_count&&!fn->params){brew_error(err,&fn->token,"function '%s' has invalid parameter storage",fn->name);return false;}}
 if(p->count>SIZE_MAX/2){brew_error(err,NULL,"function index size overflow");return false;}size_t bucket_count=16;
 while(bucket_count<p->count*2){if(bucket_count>SIZE_MAX/2){brew_error(err,NULL,"function index size overflow");return false;}bucket_count*=2;}
 if(bucket_count>SIZE_MAX/sizeof(*p->function_buckets)||p->count>SIZE_MAX/sizeof(*p->function_next)){brew_error(err,NULL,"function index size overflow");return false;}
 size_t *buckets=malloc(bucket_count*sizeof(*buckets));
 size_t *next=p->count?malloc(p->count*sizeof(*next)):NULL;
 size_t *link_buckets=malloc(bucket_count*sizeof(*link_buckets));
 size_t *link_next=p->count?malloc(p->count*sizeof(*link_next)):NULL;
 if(!buckets||!link_buckets||(p->count&&(!next||!link_next))){
  free(buckets);free(next);free(link_buckets);free(link_next);
  brew_error(err,NULL,"out of memory building function indexes");return false;
 }
 for(size_t i=0;i<bucket_count;i++){buckets[i]=SIZE_MAX;link_buckets[i]=SIZE_MAX;}
 for(size_t i=0;i<p->count;i++){
  Stmt *current=p->functions[i];size_t b=brew_name_hash(current->name)&(bucket_count-1);
  for(size_t n=buckets[b];n!=SIZE_MAX;n=next[n]){
   Stmt *prior=p->functions[n];
   if(!strcmp(prior->name,current->name)&&
      ((!prior->imported&&!current->imported)||
       (prior->imported&&current->imported&&prior->module_index==current->module_index))){
    brew_error(err,&current->token,"function '%s' is already declared in this scope",current->name);
    free(buckets);free(next);free(link_buckets);free(link_next);return false;
   }
  }
  next[i]=buckets[b];buckets[b]=i;
  const char *link_name=current->link_name?current->link_name:current->name;
  size_t lb=brew_name_hash(link_name)&(bucket_count-1);
  for(size_t n=link_buckets[lb];n!=SIZE_MAX;n=link_next[n])
   if(!strcmp(p->functions[n]->link_name?p->functions[n]->link_name:p->functions[n]->name,link_name)){
    brew_error(err,&current->token,"internal linker symbol '%s' is duplicated",link_name);
    free(buckets);free(next);free(link_buckets);free(link_next);return false;
   }
  link_next[i]=link_buckets[lb];link_buckets[lb]=i;
 }
 free(p->function_buckets);free(p->function_next);free(p->link_function_buckets);free(p->link_function_next);
 p->function_buckets=buckets;p->function_next=next;
 p->link_function_buckets=link_buckets;p->link_function_next=link_next;
 p->function_bucket_count=bucket_count;p->function_indexed=true;
 if(!brew_find_function(p,"main")){brew_error(err,NULL,"program must define fn main");return false;}
 BrewSymbols symbols={0};if(!brew_symbols_init(&symbols)){brew_error(err,NULL,"out of memory initializing semantic symbol table");return false;}bool ok=true;
 for(size_t i=0;i<p->count&&ok;i++){
  Stmt *fn=p->functions[i];if(fn->external)continue;
  brew_symbols_reset(&symbols);
  BrewSymbols local_names={0};
  if(!brew_symbols_init(&local_names)){brew_error(err,&fn->token,"out of memory initializing local declaration index");ok=false;break;}
  for(size_t a=0;a<fn->param_count;a++){
   if(!fn->params[a]||!fn->params[a][0]){brew_error(err,&fn->token,"function '%s' has an invalid parameter name",fn->name);ok=false;break;}
   if(brew_symbol_find(&symbols,fn->params[a],NULL)){brew_error(err,&fn->token,"duplicate parameter '%s'",fn->params[a]);ok=false;break;}
   bool is_array=fn->param_types&&fn->param_types[a]&&!strcmp(fn->param_types[a],"u32_array");
   if(!brew_symbol_add(&symbols,fn->params[a],false,true,is_array)||
      !brew_symbol_add(&local_names,fn->params[a],false,true,is_array)){
    brew_error(err,&fn->token,"out of memory adding function parameter");ok=false;break;
   }
  }
  if(ok)ok=brew_validate_local_names(fn->body,fn->body_count,&local_names,err);
  brew_symbols_destroy(&local_names);
  if(ok)ok=brew_check_block(p,fn->body,fn->body_count,&symbols,err,0);
 }
 brew_symbols_destroy(&symbols);return ok;
}
#endif
