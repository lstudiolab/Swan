#ifndef SWAN_CORE_H
#define SWAN_CORE_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
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
#include <time.h>
typedef enum {
 SWAN_PROFILE_READ, SWAN_PROFILE_LEX, SWAN_PROFILE_PARSE, SWAN_PROFILE_IMPORTS,
 SWAN_PROFILE_SEMA, SWAN_PROFILE_REACHABILITY, SWAN_PROFILE_IRGEN,
 SWAN_PROFILE_IR_OPT, SWAN_PROFILE_CODEGEN, SWAN_PROFILE_AST_CODEGEN,
 SWAN_PROFILE_ASM_WRITE, SWAN_PROFILE_LINK, SWAN_PROFILE_RUN,
 SWAN_PROFILE_COUNT
} SwanProfileStage;
static const char *const swan_profile_names[SWAN_PROFILE_COUNT]={
 "Source file read","Lexing","Parsing","Import loading",
 "Semantic analysis","Reachability analysis","IR generation",
 "IR optimization","Target code generation","AST fallback code generation",
 "Assembly output","Native linking","Program execution"
};
static bool swan_profile_enabled=false;
static uint64_t swan_profile_ns[SWAN_PROFILE_COUNT];
static struct timespec swan_profile_total_start;
static inline uint64_t swan_profile_clock(void){
 struct timespec t;if(timespec_get(&t,TIME_UTC)!=TIME_UTC)return 0;
 return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}
static inline uint64_t swan_profile_start(void){return swan_profile_enabled?swan_profile_clock():0;}
static inline void swan_profile_end(SwanProfileStage stage,uint64_t start){
 if(swan_profile_enabled&&start&&stage<SWAN_PROFILE_COUNT){uint64_t end=swan_profile_clock();if(end>=start)swan_profile_ns[stage]+=end-start;}
}
static inline void swan_profile_reset(void){
 memset(swan_profile_ns,0,sizeof(swan_profile_ns));
 if(timespec_get(&swan_profile_total_start,TIME_UTC)!=TIME_UTC)memset(&swan_profile_total_start,0,sizeof(swan_profile_total_start));
}
static inline void swan_profile_print(void){
 if(!swan_profile_enabled)return;
 uint64_t total=swan_profile_clock(),begin=(uint64_t)swan_profile_total_start.tv_sec*UINT64_C(1000000000)+(uint64_t)swan_profile_total_start.tv_nsec;
 uint64_t elapsed=total>=begin?total-begin:0;
 fprintf(stderr,"\nSwan compiler pipeline profile (seconds)\n");
 for(size_t i=0;i<SWAN_PROFILE_COUNT;i++)if(swan_profile_ns[i]){
  double seconds=(double)swan_profile_ns[i]/1e9;
  fprintf(stderr,"  %-30s %0.9f s  (%5.1f%%)\n",swan_profile_names[i],seconds,elapsed?(double)swan_profile_ns[i]*100.0/(double)elapsed:0.0);
 }
 fprintf(stderr,"  %-30s %0.9f s\n","Total wall time",(double)elapsed/1e9);
}
#define SWAN_VERSION "0.8.0"
#define SWAN_MEMORY_MODEL "Automatic cleanup; borrow checking incomplete"
#define SWAN_MAX_TOKENS 131072
#define SWAN_MAX_NODES 32768
#define SWAN_MAX_TEXT 512
typedef enum {
 T_EOF,T_IDENT,T_INT,T_FLOAT,T_STRING,T_NEWLINE,T_INDENT,T_DEDENT,
 T_FN,T_IMPORT,T_EXTERN,T_LET,T_VAR,T_CONST,T_RETURN,T_IF,T_ELSE,T_WHILE,T_BREAK,T_CONTINUE,T_PASS,
 T_TRUE,T_FALSE,T_NONE,T_AND,T_OR,T_NOT,T_BANG,T_LP,T_RP,T_COLON,T_COMMA,T_ARROW,
 T_PLUS,T_MINUS,T_STAR,T_SLASH,T_FLOORDIV,T_PERCENT,T_ASSIGN,T_EQ,T_NE,T_LT,T_GT,T_LE,T_GE,
 T_LB,T_RB,T_DOT,T_SEMI,T_OUT,T_PRINT,T_STRUCT,T_ENUM,T_TYPE,T_FOR,T_IN,T_LOOP,T_MATCH,T_GUARD,T_FROM,T_AS,T_EXPORT,T_SELF,T_LAZY,T_INIT,T_DEINIT,T_FREE,T_THROW,T_RETHROW,T_STORE,T_BORROW,
 T_NAMESPACE,T_VARIANT,T_PROTOCOL,T_THREAD,T_ASYNC,T_BLOCK,T_SET,T_BOOL_TYPE,T_CASE,T_DEFER,T_MANUAL,T_MOD,T_UPTO,T_INLINE,T_STATIC,T_PRIVATE,T_PUBLIC
} TokenKind;
typedef struct { TokenKind kind; const char *text; unsigned line,col; } Token;
typedef struct { Token *items; size_t count,capacity,pos; const char *source; size_t length; char *text_storage; size_t text_used,text_capacity; } TokenStream;
typedef struct SwanArenaChunk { struct SwanArenaChunk *next; size_t used,capacity; max_align_t alignment; unsigned char data[]; } SwanArenaChunk;
typedef struct { char message[1024]; unsigned line,col; } SwanError;
static inline void swan_error(SwanError *e,const Token *t,const char *fmt,...) {
 va_list ap; va_start(ap,fmt); vsnprintf(e->message,sizeof(e->message),fmt,ap); va_end(ap);
 e->line=t?t->line:0; e->col=t?t->col:0;
}
#endif
