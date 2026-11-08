#ifndef SWAN_CORE_H
#define SWAN_CORE_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SWAN_VERSION "0.7.0-samc"
#define SWAN_MEMORY_MODEL "SAMC"
#define SWAN_MAX_TOKENS 131072
#define SWAN_MAX_NODES 32768
#define SWAN_MAX_TEXT 512
typedef enum { T_EOF,T_IDENT,T_INT,T_FLOAT,T_STRING,T_NEWLINE,T_INDENT,T_DEDENT,
 T_FN,T_IMPORT,T_LET,T_VAR,T_CONST,T_MANUAL,T_RETURN,T_LOG,T_IF,T_ELSE,T_WHILE,T_BREAK,T_CONTINUE,T_PASS,
 T_TRUE,T_FALSE,T_NONE,T_AND,T_OR,T_NOT,T_BANG,T_LP,T_RP,T_COLON,T_COMMA,T_ARROW,
 T_PLUS,T_MINUS,T_STAR,T_SLASH,T_FLOORDIV,T_PERCENT,T_ASSIGN,T_EQ,T_NE,T_LT,T_GT,T_LE,T_GE,
 T_LB,T_RB,T_DOT,T_SEMI } TokenKind;
typedef struct { TokenKind kind; const char *text; unsigned line,col; } Token;
typedef struct { Token *items; size_t count,capacity,pos; const char *source; size_t length; char *text_storage; size_t text_used,text_capacity; } TokenStream;
typedef enum { E_INT,E_FLOAT,E_STRING,E_BOOL,E_NAME,E_UNARY,E_BINARY,E_CALL,E_INDEX } ExprKind;
typedef struct Expr Expr;
struct Expr { ExprKind kind; Token token; Expr *left,*right; Expr **args; size_t argc; bool contains_call,constant_checked,constant_valid; int64_t constant_value; };
typedef enum { S_FUNCTION,S_VARIABLE,S_LOG,S_RETURN,S_IF,S_WHILE,S_BREAK,S_CONTINUE,S_PASS,S_EXPR } StmtKind;
typedef struct Stmt Stmt;
struct Stmt { StmtKind kind; Token token; const char *name,*type; bool mutable_value,manual_memory; Expr *value,*condition; Stmt **body,**alternate; size_t body_count,alt_count,body_cap,alt_cap; const char **params; size_t param_count; Stmt *next; };
typedef struct SwanArenaChunk { struct SwanArenaChunk *next; size_t used,capacity; max_align_t alignment; unsigned char data[]; } SwanArenaChunk;
typedef struct { Stmt *functions[1024]; size_t count; const char *imports[64]; size_t import_count; SwanArenaChunk *arena; int function_buckets[2048]; unsigned short function_next[1024]; bool function_indexed; } Program;
static inline void *swan_arena_alloc(Program *p,size_t size){
 const size_t align=_Alignof(max_align_t);size=(size+align-1)&~(align-1);
 SwanArenaChunk *c=p->arena;
 if(!c||size>c->capacity-c->used){
  size_t cap=size>65536?size:65536;
  c=malloc(sizeof(*c)+cap);if(!c)return NULL;c->next=p->arena;c->used=0;c->capacity=cap;p->arena=c;
 }
 void *result=c->data+c->used;c->used+=size;memset(result,0,size);return result;
}
static inline void swan_program_destroy(Program *p){
 SwanArenaChunk *c=p->arena;while(c){SwanArenaChunk *next=c->next;free(c);c=next;}p->arena=NULL;
}
typedef struct { char message[1024]; unsigned line,col; } SwanError;
static inline void swan_error(SwanError *e,const Token *t,const char *fmt,...) {
 va_list ap; va_start(ap,fmt); vsnprintf(e->message,sizeof(e->message),fmt,ap); va_end(ap);
 e->line=t?t->line:0; e->col=t?t->col:0;
}
#endif
