#ifndef SWAN_AST_H
#define SWAN_AST_H
#include "swan/core.h"
typedef enum { E_INT,E_FLOAT,E_STRING,E_BOOL,E_NAME,E_UNARY,E_BINARY,E_CALL,E_INDEX,E_MEMBER } ExprKind;
typedef struct Expr Expr;
struct Expr { ExprKind kind; Token token; Expr *left,*right; Expr **args; size_t argc,arg_capacity; bool contains_call,constant_checked,constant_valid; int64_t constant_value; };
static inline const char *swan_expr_terminal_name(Expr *e){while(e&&e->kind==E_MEMBER)e=e->right;return e&&e->kind==E_NAME?e->token.text:NULL;}
typedef enum { S_FUNCTION,S_VARIABLE,S_LOG,S_RETURN,S_IF,S_WHILE,S_BREAK,S_CONTINUE,S_PASS,S_EXPR,S_FREE,S_DROP } StmtKind;
typedef struct Stmt Stmt;
struct Stmt { StmtKind kind; Token token; size_t source_index; const char *name,*link_name,*type; bool mutable_value,manual_memory,manual_checked,manual_borrow,exported; Expr *value,*condition; Stmt **body,**alternate; size_t body_count,alt_count,body_cap,alt_cap; const char **params; const char **param_types; size_t param_count,param_capacity; Stmt *next; int module_index; bool imported,external; };
typedef struct { char *path,*text,*token_storage; } SwanSourceFile;
typedef struct {
 Stmt **functions; size_t count,function_capacity;
 const char **imports; size_t import_count,import_capacity;
 const char **c_headers; size_t c_header_count,c_header_capacity;
 SwanArenaChunk *arena;
 size_t *function_buckets,*function_next,*link_function_buckets,*link_function_next,function_bucket_count;
 bool function_indexed;
 SwanSourceFile *source_files; size_t source_count,source_capacity,current_source_index;
 size_t source_function_count,module_count,module_capacity;
 size_t *module_starts,*module_ends; size_t module_starts_capacity,module_ends_capacity;
 const char **module_names;
 bool *function_reachable;
 bool reachability_computed,bundle_imports,codegen_force_ast,codegen_include_external_functions,no_heap,raw_mode;
 int codegen_module_index,codegen_source_index;
 SwanWarning *warnings; size_t warning_count,warning_capacity,warnings_dropped;
} Program;
static inline bool swan_reserve_array(void **items,size_t *capacity,size_t needed,size_t item_size){
 if(!items||!capacity||!item_size||needed>SIZE_MAX/item_size)return false;
 if(needed<=*capacity)return true;
 size_t next=*capacity?*capacity:8;
 while(next<needed){if(next>SIZE_MAX/2){next=needed;break;}next*=2;}
 if(next>SIZE_MAX/item_size)return false;
 void *memory=realloc(*items,next*item_size);
 if(!memory)return false;
 *items=memory;*capacity=next;return true;
}
static inline bool swan_program_add_function(Program *p,Stmt *function){
 if(!p||!function||p->count==SIZE_MAX||
    !swan_reserve_array((void**)&p->functions,&p->function_capacity,p->count+1,sizeof(*p->functions)))return false;
 p->functions[p->count++]=function;p->function_indexed=false;return true;
}
static inline bool swan_program_add_import(Program *p,const char *name){
 if(!p||!name||p->import_count==SIZE_MAX||
    !swan_reserve_array((void**)&p->imports,&p->import_capacity,p->import_count+1,sizeof(*p->imports)))return false;
 p->imports[p->import_count++]=name;return true;
}
static inline bool swan_program_add_c_header(Program *p,const char *name){
 if(!p||!name||p->c_header_count==SIZE_MAX||
    !swan_reserve_array((void**)&p->c_headers,&p->c_header_capacity,p->c_header_count+1,sizeof(*p->c_headers)))return false;
 p->c_headers[p->c_header_count++]=name;return true;
}
static inline bool swan_program_add_module(Program *p,const char *name,size_t start,size_t *index){
 if(!p||!name||p->module_count==SIZE_MAX)return false;
 size_t needed=p->module_count+1;
 if(!swan_reserve_array((void**)&p->module_names,&p->module_capacity,needed,sizeof(*p->module_names)))return false;
 if(!swan_reserve_array((void**)&p->module_starts,&p->module_starts_capacity,needed,sizeof(*p->module_starts))||
    !swan_reserve_array((void**)&p->module_ends,&p->module_ends_capacity,needed,sizeof(*p->module_ends)))return false;
 p->module_starts[p->module_count]=start;p->module_ends[p->module_count]=start;
 p->module_names[p->module_count]=name;if(index)*index=p->module_count;p->module_count++;return true;
}
static inline bool swan_program_add_source(Program *p,const char *path,char *text,char *token_storage){
 if(!p||!path||!text)return false;
 if(p->source_count==SIZE_MAX||
    !swan_reserve_array((void**)&p->source_files,&p->source_capacity,
                        p->source_count+1,sizeof(*p->source_files)))return false;
 SwanSourceFile *source=&p->source_files[p->source_count++];
 size_t path_length=strlen(path);
 source->path=malloc(path_length+1);
 if(source->path)memcpy(source->path,path,path_length+1);
 source->text=text;source->token_storage=token_storage;
 if(!source->path){p->source_count--;return false;}
 return true;
}
static inline void *swan_arena_alloc(Program *p,size_t size){
 if(!p)return NULL;
 const size_t align=_Alignof(max_align_t);
 if(size>SIZE_MAX-(align-1))return NULL;
 size=(size+align-1)&~(align-1);
 SwanArenaChunk *c=p->arena;
 if(!c||c->used>c->capacity||size>c->capacity-c->used){
  size_t cap=size>65536?size:65536;
  if(cap>SIZE_MAX-sizeof(*c))return NULL;
  c=malloc(sizeof(*c)+cap);
  if(!c)return NULL;
  c->next=p->arena;c->used=0;c->capacity=cap;p->arena=c;
 }
 void *result=c->data+c->used;
 c->used+=size;
 memset(result,0,size);
 return result;
}
static inline bool swan_arena_grow(Program *p,void **items,size_t old_count,size_t new_count,size_t item_size){
 if(!p||!items||!item_size||new_count<old_count||new_count>SIZE_MAX/item_size)return false;
 void *next=swan_arena_alloc(p,new_count*item_size);
 if(!next)return false;
 if(old_count)memcpy(next,*items,old_count*item_size);
 *items=next;return true;
}
static inline void swan_program_destroy(Program *p){
 if(!p)return;
 for(size_t i=0;i<p->source_count;i++){
  free(p->source_files[i].path);free(p->source_files[i].text);free(p->source_files[i].token_storage);
 }
 free(p->source_files);free(p->functions);free(p->imports);free(p->c_headers);free(p->module_names);free(p->module_starts);free(p->module_ends);free(p->function_buckets);free(p->function_next);free(p->link_function_buckets);free(p->link_function_next);free(p->function_reachable);free(p->warnings);
 SwanArenaChunk *chunk=p->arena;
 while(chunk){SwanArenaChunk *next=chunk->next;free(chunk);chunk=next;}
 memset(p,0,sizeof(*p));
}
#endif
