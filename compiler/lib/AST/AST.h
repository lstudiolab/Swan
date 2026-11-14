#ifndef SWAN_AST_H
#define SWAN_AST_H
#include "swan/core.h"
typedef enum { E_INT,E_FLOAT,E_STRING,E_BOOL,E_NAME,E_UNARY,E_BINARY,E_CALL,E_INDEX,E_MEMBER } ExprKind;
typedef struct Expr Expr;
struct Expr { ExprKind kind; Token token; Expr *left,*right; Expr **args; size_t argc; bool contains_call,constant_checked,constant_valid; int64_t constant_value; };
static inline const char *swan_expr_terminal_name(Expr *e){while(e&&e->kind==E_MEMBER)e=e->right;return e&&e->kind==E_NAME?e->token.text:NULL;}
typedef enum { S_FUNCTION,S_VARIABLE,S_LOG,S_RETURN,S_IF,S_WHILE,S_BREAK,S_CONTINUE,S_PASS,S_EXPR,S_FREE } StmtKind;
typedef struct Stmt Stmt;
struct Stmt { StmtKind kind; Token token; const char *name,*type; bool mutable_value,manual_memory,manual_checked,exported; Expr *value,*condition; Stmt **body,**alternate; size_t body_count,alt_count,body_cap,alt_cap; const char **params; const char **param_types; size_t param_count; Stmt *next; int module_index; bool imported,external; };
typedef struct { Stmt *functions[1024]; size_t count; const char *imports[64]; size_t import_count; const char *c_headers[64]; size_t c_header_count; SwanArenaChunk *arena; int function_buckets[2048]; unsigned short function_next[1024]; bool function_indexed; size_t source_function_count,module_count,module_starts[64],module_ends[64]; const char *module_names[64]; bool function_reachable[1024]; bool reachability_computed,bundle_imports; int codegen_module_index; SwanWarning warnings[128]; size_t warning_count,warnings_dropped; } Program;
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
static inline void swan_program_destroy(Program *p){
 SwanArenaChunk *c=p->arena;while(c){SwanArenaChunk *next=c->next;free(c);c=next;}p->arena=NULL;
}
#endif
