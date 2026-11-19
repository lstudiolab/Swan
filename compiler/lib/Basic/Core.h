#ifndef BREW_CORE_H
#define BREW_CORE_H
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
 BREW_PROFILE_READ, BREW_PROFILE_LEX, BREW_PROFILE_PARSE, BREW_PROFILE_IMPORTS,
 BREW_PROFILE_SEMA, BREW_PROFILE_REACHABILITY, BREW_PROFILE_SIL, BREW_PROFILE_IRGEN,
 BREW_PROFILE_IR_OPT, BREW_PROFILE_CODEGEN, BREW_PROFILE_AST_CODEGEN,
 BREW_PROFILE_ASM_WRITE, BREW_PROFILE_LINK, BREW_PROFILE_RUN,
 BREW_PROFILE_IR_VERIFY, BREW_PROFILE_IR_PASS_CONSTANT_FOLD,
 BREW_PROFILE_IR_PASS_CFG, BREW_PROFILE_IR_PASS_ALGEBRAIC,
 BREW_PROFILE_IR_PASS_COPY, BREW_PROFILE_IR_PASS_CSE,
 BREW_PROFILE_IR_PASS_LICM, BREW_PROFILE_IR_PASS_DCE,
 BREW_PROFILE_COUNT
} BrewProfileStage;
static const char *const brew_profile_names[BREW_PROFILE_COUNT]={
 "Source file read","Lexing","Parsing","Import loading",
 "Semantic analysis","Reachability analysis","SIL generation and optimization","IR generation",
 "IR optimization","Target code generation","AST fallback code generation",
 "Assembly output","Native linking","Program execution",
 "IR verification (inclusive)","IR pass: constant folding",
 "IR pass: CFG simplification","IR pass: algebraic simplification",
 "IR pass: copy propagation","IR pass: common-subexpression elimination",
 "IR pass: loop-invariant code motion","IR pass: dead-code elimination"
};
static bool brew_profile_enabled=false;
static uint64_t brew_profile_ns[BREW_PROFILE_COUNT];
static size_t brew_profile_ir_pass_executions;
static size_t brew_profile_ir_pass_rounds;
#define BREW_COMPILER_OPTIMIZATION_BUDGET 350u
static size_t brew_compiler_optimization_passes;
static inline void brew_optimizer_reset(void){brew_compiler_optimization_passes=0;}
static inline bool brew_optimizer_consume(size_t count){
 if(count>BREW_COMPILER_OPTIMIZATION_BUDGET-brew_compiler_optimization_passes)return false;
 brew_compiler_optimization_passes+=count;return true;
}
static struct timespec brew_profile_total_start;
static inline bool brew_profile_time(struct timespec *t){
#if defined(BREW_USE_MONOTONIC_CLOCK) && defined(CLOCK_MONOTONIC)
 return clock_gettime(CLOCK_MONOTONIC,t)==0;
#else
 return timespec_get(t,TIME_UTC)==TIME_UTC;
#endif
}
static inline uint64_t brew_profile_clock(void){
 struct timespec t;if(!brew_profile_time(&t))return 0;
 return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}
static inline uint64_t brew_profile_start(void){return brew_profile_enabled?brew_profile_clock():0;}
static inline void brew_profile_end(BrewProfileStage stage,uint64_t start){
 if(brew_profile_enabled&&start&&stage<BREW_PROFILE_COUNT){uint64_t end=brew_profile_clock();if(end>=start)brew_profile_ns[stage]+=end-start;}
}
static inline void brew_profile_reset(void){
 memset(brew_profile_ns,0,sizeof(brew_profile_ns));
 brew_profile_ir_pass_executions=0;
 brew_profile_ir_pass_rounds=0;
 if(!brew_profile_time(&brew_profile_total_start))memset(&brew_profile_total_start,0,sizeof(brew_profile_total_start));
}
static inline void brew_profile_print(void){
 if(!brew_profile_enabled)return;
 uint64_t total=brew_profile_clock(),begin=(uint64_t)brew_profile_total_start.tv_sec*UINT64_C(1000000000)+(uint64_t)brew_profile_total_start.tv_nsec;
 uint64_t elapsed=total>=begin?total-begin:0;
 fprintf(stderr,"\nBrew compiler pipeline profile (seconds)\n");
 for(size_t i=0;i<BREW_PROFILE_COUNT;i++)if(brew_profile_ns[i]){
  double seconds=(double)brew_profile_ns[i]/1e9;
  fprintf(stderr,"  %-30s %0.9f s  (%5.1f%%)\n",brew_profile_names[i],seconds,elapsed?(double)brew_profile_ns[i]*100.0/(double)elapsed:0.0);
 }
 fprintf(stderr,"  %-30s %0.9f s\n","Total wall time",(double)elapsed/1e9);
 fprintf(stderr,"  IR pass executions: %zu (rounds: %zu)\n",brew_profile_ir_pass_executions,brew_profile_ir_pass_rounds);
 fprintf(stderr,"  Compiler-wide optimization passes: %zu / %u\n",brew_compiler_optimization_passes,BREW_COMPILER_OPTIMIZATION_BUDGET);
}
#define BREW_VERSION "0.8.0"
#define BREW_MEMORY_MODEL "Automatic cleanup; borrow checking incomplete"
#define BREW_MAX_TOKENS 131072
#define BREW_MAX_NODES 32768
#define BREW_MAX_TEXT 512
typedef enum {
 T_EOF,T_IDENT,T_INT,T_FLOAT,T_STRING,T_NEWLINE,T_INDENT,T_DEDENT,
 T_FN,T_IMPORT,T_EXTERN,T_LET,T_VAR,T_CONST,T_RETURN,T_IF,T_ELSE,T_WHILE,T_BREAK,T_CONTINUE,T_PASS,
 T_TRUE,T_FALSE,T_NONE,T_AND,T_OR,T_NOT,T_BANG,T_LP,T_RP,T_COLON,T_COMMA,T_ARROW,
 T_PLUS,T_MINUS,T_STAR,T_SLASH,T_FLOORDIV,T_PERCENT,T_ASSIGN,T_EQ,T_NE,T_LT,T_GT,T_LE,T_GE,
 T_LB,T_RB,T_DOT,T_SEMI,T_OUT,T_PRINT,T_STRUCT,T_ENUM,T_TYPE,T_FOR,T_IN,T_LOOP,T_MATCH,T_GUARD,T_FROM,T_AS,T_EXPORT,T_SELF,T_LAZY,T_INIT,T_DEINIT,T_THROW,T_RETHROW,T_STORE,T_BORROW,
 T_NAMESPACE,T_VARIANT,T_PROTOCOL,T_THREAD,T_ASYNC,T_BLOCK,T_SET,T_BOOL_TYPE,T_CASE,T_DEFER,T_MANUAL,T_MOD,T_UPTO,T_INLINE,T_STATIC,T_PRIVATE,T_PUBLIC,T_THROWS,T_CATCH
} TokenKind;
typedef struct { TokenKind kind; const char *text; unsigned line,col; const char *source_name; } Token;
typedef struct { Token *items; size_t count,capacity,pos; const char *source; size_t length; char *text_storage; size_t text_used,text_capacity; } TokenStream;
typedef struct BrewArenaChunk { struct BrewArenaChunk *next; size_t used,capacity; max_align_t alignment; unsigned char data[]; } BrewArenaChunk;
typedef struct { char message[1024]; unsigned line,col; const char *source_name; } BrewError;
typedef struct { char message[256]; unsigned line,col; const char *source_name; } BrewWarning;
static inline void brew_error(BrewError *e,const Token *t,const char *fmt,...) {
 if(!e)return;
 va_list ap;
 va_start(ap,fmt);
 vsnprintf(e->message,sizeof(e->message),fmt,ap);
 va_end(ap);
 e->line=t?t->line:0;
 e->col=t?t->col:0;
 e->source_name=t?t->source_name:NULL;
}
#endif
