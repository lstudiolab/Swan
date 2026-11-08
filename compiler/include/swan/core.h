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
typedef struct SwanArenaChunk { struct SwanArenaChunk *next; size_t used,capacity; max_align_t alignment; unsigned char data[]; } SwanArenaChunk;
typedef struct { char message[1024]; unsigned line,col; } SwanError;
static inline void swan_error(SwanError *e,const Token *t,const char *fmt,...) {
 va_list ap; va_start(ap,fmt); vsnprintf(e->message,sizeof(e->message),fmt,ap); va_end(ap);
 e->line=t?t->line:0; e->col=t?t->col:0;
}
#endif
