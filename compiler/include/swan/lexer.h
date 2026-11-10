#ifndef SWAN_LEXER_H
#define SWAN_LEXER_H
#include "swan/core.h"

/* Swan source is UTF-8. Validate once, then scan UTF-8 bytes without
 * locale-dependent character classification. Columns remain byte-based. */
static inline size_t swan_utf8_width(const unsigned char *s) {
 unsigned char c=s[0];
 if(c<0x80)return 1;
 if(c>=0xC2&&c<=0xDF)return 2;
 if(c>=0xE0&&c<=0xEF)return 3;
 if(c>=0xF0&&c<=0xF4)return 4;
 return 0;
}
static inline bool swan_utf8_valid(const unsigned char *s,size_t n,size_t *bad) {
 size_t i=0;
 while(i<n){
  size_t width=swan_utf8_width(s+i);
  if(!width||width>n-i){if(bad)*bad=i;
    return false;}
  if(width>1){
   for(size_t j=1;j<width;j++)if((s[i+j]&0xC0)!=0x80){if(bad)*bad=i+j;return false;}
   if((s[i]==0xE0&&s[i+1]<0xA0)||(s[i]==0xED&&s[i+1]>=0xA0)||
      (s[i]==0xF0&&s[i+1]<0x90)||(s[i]==0xF4&&s[i+1]>=0x90)){
    if(bad)*bad=i;return false;
   }
  }
  i+=width;
 }
 return true;
}
static inline bool swan_ascii_alpha(unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');}
static inline bool swan_ascii_digit(unsigned char c){return c>='0'&&c<='9';}
static inline bool swan_ident_start(unsigned char c){return swan_ascii_alpha(c)||c=='_'||c>=0x80;}
static inline bool swan_ident_continue(unsigned char c){return swan_ident_start(c)||swan_ascii_digit(c);}
static inline TokenKind swan_keyword(const char *s,size_t n) {
 /* Length-dispatched lookup avoids allocation and hashing in the lexer hot path. */
 switch(n) {
 case 2:
  if(!memcmp(s,"fn",2))return T_FN; /* deprecated source-compatibility alias; func is canonical */
  if(!memcmp(s,"if",2))return T_IF;
  if(!memcmp(s,"in",2))return T_IN;
  if(!memcmp(s,"as",2))return T_AS;
  break;
 case 3:
  if(!memcmp(s,"let",3))return T_LET;
  if(!memcmp(s,"var",3))return T_VAR;
  if(!memcmp(s,"for",3))return T_FOR;
  if(!memcmp(s,"nil",3))return T_NONE;
  if(!memcmp(s,"out",3))return T_OUT; /* deprecated block-terminator compatibility */
  if(!memcmp(s,"set",3))return T_SET;
  break;
 case 4:
  if(!memcmp(s,"func",4))return T_FN;
  if(!memcmp(s,"else",4))return T_ELSE;
  if(!memcmp(s,"enum",4))return T_ENUM;
  if(!memcmp(s,"type",4))return T_TYPE;
  if(!memcmp(s,"loop",4))return T_LOOP;
  if(!memcmp(s,"from",4))return T_FROM;
  if(!memcmp(s,"self",4))return T_SELF;
  if(!memcmp(s,"true",4))return T_TRUE;
  if(!memcmp(s,"pass",4))return T_PASS;
  if(!memcmp(s,"null",4))return T_NONE;
  if(!memcmp(s,"bool",4))return T_BOOL_TYPE;
  if(!memcmp(s,"lazy",4))return T_LAZY;
  if(!memcmp(s,"case",4))return T_CASE;
  break;
 case 5:
  if(!memcmp(s,"while",5))return T_WHILE;
  if(!memcmp(s,"match",5))return T_MATCH;
  if(!memcmp(s,"guard",5))return T_GUARD;
  if(!memcmp(s,"break",5))return T_BREAK;
  if(!memcmp(s,"false",5))return T_FALSE;
  if(!memcmp(s,"store",5))return T_STORE; /* deprecated manual-memory syntax alias */
  if(!memcmp(s,"const",5))return T_CONST;
  if(!memcmp(s,"block",5))return T_BLOCK;
  if(!memcmp(s,"defer",5))return T_DEFER;
  if(!memcmp(s,"async",5))return T_ASYNC;
  break;
 case 6:
  if(!memcmp(s,"manual",6))return T_MANUAL;
  if(!memcmp(s,"import",6))return T_IMPORT;
  if(!memcmp(s,"export",6))return T_EXPORT;
  if(!memcmp(s,"return",6))return T_RETURN;
  if(!memcmp(s,"struct",6))return T_STRUCT;
  if(!memcmp(s,"thread",6))return T_THREAD;
  break;
 case 7:
  if(!memcmp(s,"variant",7))return T_VARIANT;
  break;
 case 8:
  if(!memcmp(s,"external",8))return T_EXTERN;
  if(!memcmp(s,"continue",8))return T_CONTINUE;
  if(!memcmp(s,"protocol",8))return T_PROTOCOL;
  break;
 case 9:
  if(!memcmp(s,"namespace",9))return T_NAMESPACE;
  break;
 }
 return T_IDENT;
}
static inline bool swan_emit(TokenStream *ts,TokenKind k,const char *s,size_t n,unsigned line,unsigned col,SwanError *err) {
 if(ts->count>=SWAN_MAX_TOKENS){Token t={0};swan_error(err,&t,"source has too many tokens");return false;}
 if(ts->count==ts->capacity){size_t cap=ts->capacity?ts->capacity*2:256;if(cap>SWAN_MAX_TOKENS)cap=SWAN_MAX_TOKENS;Token *items=realloc(ts->items,cap*sizeof(*items));if(!items){swan_error(err,NULL,"out of memory while tokenizing");return false;}ts->items=items;ts->capacity=cap;}
 if(n>=SWAN_MAX_TEXT){Token t={0};t.line=line;t.col=col;swan_error(err,&t,"token is too long");return false;}
 if(n&&n+1>ts->text_capacity-ts->text_used){size_t need=ts->text_used+n+1,cap=ts->text_capacity?ts->text_capacity:64;while(cap<need){if(cap>SIZE_MAX/2){cap=need;break;}cap*=2;}char *storage=realloc(ts->text_storage,cap);if(!storage){swan_error(err,NULL,"out of memory storing token text");return false;}ts->text_storage=storage;ts->text_capacity=cap;}
 Token *t=&ts->items[ts->count++];t->kind=k;t->line=line;t->col=col;
 if(n){t->text=ts->text_storage+ts->text_used;memcpy(ts->text_storage+ts->text_used,s,n);ts->text_storage[ts->text_used+n]=0;ts->text_used+=n+1;}else t->text="";return true;
}
static inline bool swan_lex(const char *src,TokenStream *ts,SwanError *err) {
 memset(ts,0,sizeof(*ts));ts->source=src;ts->length=strlen(src);
 size_t bad=0;
 if(!swan_utf8_valid((const unsigned char*)src,ts->length,&bad)){swan_error(err,NULL,"source is not valid UTF-8 at byte offset %zu",bad);return false;}
 if(ts->length>(SIZE_MAX-16)/2){swan_error(err,NULL,"source file is too large");return false;}
 ts->text_capacity=ts->length*2+16;ts->text_storage=malloc(ts->text_capacity);
 if(!ts->text_storage){swan_error(err,NULL,"out of memory allocating token text");return false;}
 size_t i=0;unsigned line=1,col=1;unsigned indents[128]={0};size_t depth=1;bool line_start=true;
 /* Accept and strip an optional UTF-8 BOM at the beginning of a source file. */
 if(ts->length>=3&&(unsigned char)src[0]==0xEF&&(unsigned char)src[1]==0xBB&&(unsigned char)src[2]==0xBF)i=3;
 while(i<ts->length){
  if(line_start){size_t start=i;unsigned width=0;while(i<ts->length&&(src[i]==' '||src[i]=='\t')){width+=src[i]=='\t'?4:1;i++;}
   col+=(unsigned)(i-start);
   if(i==ts->length||src[i]=='\n'||src[i]=='#'){while(i<ts->length&&src[i]!='\n'){i++;col++;}if(i<ts->length){i++;line++;col=1;}continue;}
   line_start=false;
   if(width>indents[depth-1]){if(depth>=128){swan_error(err,NULL,"indentation nesting too deep");return false;}indents[depth++]=width;if(!swan_emit(ts,T_INDENT,"",0,line,col,err))return false;}
   else if(width<indents[depth-1]){while(depth>1&&width<indents[depth-1]){depth--;if(!swan_emit(ts,T_DEDENT,"",0,line,col,err))return false;}if(width!=indents[depth-1]){Token t={0};t.line=line;t.col=col;swan_error(err,&t,"inconsistent indentation");return false;}}
  }
  char c=src[i];if(c==' '||c=='\t'||c=='\r'){i++;col++;continue;}
  if(c=='#'){while(i<ts->length&&src[i]!='\n'){i++;col++;}continue;}
  if(c=='\n'){if(!swan_emit(ts,T_NEWLINE,"",0,line,col,err))return false;i++;line++;col=1;line_start=true;continue;}
  unsigned sl=line,sc=col;size_t st=i;
  if(swan_ident_start((unsigned char)c)){while(i<ts->length&&swan_ident_continue((unsigned char)src[i])){size_t width=(unsigned char)src[i]>=0x80?swan_utf8_width((const unsigned char*)src+i):1;i+=width;col+=(unsigned)width;}size_t n=i-st;if(n>=SWAN_MAX_TEXT){Token t={0};t.line=sl;t.col=sc;swan_error(err,&t,"identifier too long");return false;}if(!swan_emit(ts,swan_keyword(src+st,n),src+st,n,sl,sc,err))return false;continue;}
  if(swan_ascii_digit((unsigned char)c)){while(i<ts->length&&swan_ascii_digit((unsigned char)src[i])){i++;col++;}TokenKind k=T_INT;if(i+1<ts->length&&src[i]=='.'&&swan_ascii_digit((unsigned char)src[i+1])){k=T_FLOAT;i++;col++;while(i<ts->length&&swan_ascii_digit((unsigned char)src[i])){i++;col++;}}if(!swan_emit(ts,k,src+st,i-st,sl,sc,err))return false;continue;}
  if(c=='"'||c=='\''){char quote=c;i++;col++;char value[SWAN_MAX_TEXT];size_t n=0;bool closed=false;while(i<ts->length){c=src[i++];col++;if(c==quote){closed=true;break;}if(c=='\\'){if(i>=ts->length)break;c=src[i++];col++;switch(c){case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;case '\\':case '"':case '\'':break;default:{Token t={0};t.line=sl;t.col=sc;swan_error(err,&t,"unknown string escape");return false;}}}if(n+1>=sizeof(value)){Token t={0};t.line=sl;t.col=sc;swan_error(err,&t,"string literal too long");return false;}value[n++]=c;}if(!closed){Token t={0};t.line=sl;t.col=sc;swan_error(err,&t,"unterminated string");return false;}if(!swan_emit(ts,T_STRING,value,n,sl,sc,err))return false;continue;}
  TokenKind k=T_EOF;size_t n=1;
  if(i+1<ts->length){char d=src[i+1];if(c=='-'&&d=='>'){k=T_ARROW;n=2;}else if(c=='/'&&d=='/'){k=T_FLOORDIV;n=2;}else if(c=='='&&d=='='){k=T_EQ;n=2;}else if(c=='!'&&d=='='){k=T_NE;n=2;}else if(c=='<'&&d=='='){k=T_LE;n=2;}else if(c=='>'&&d=='='){k=T_GE;n=2;}}
  if(k==T_EOF)switch(c){case '(':k=T_LP;break;case ')':k=T_RP;break;case ':':k=T_COLON;break;case ',':k=T_COMMA;break;case '+':k=T_PLUS;break;case '-':k=T_MINUS;break;case '*':k=T_STAR;break;case '/':k=T_SLASH;break;case '%':k=T_PERCENT;break;case '=':k=T_ASSIGN;break;case '!':k=T_BANG;break;case '<':k=T_LT;break;case '>':k=T_GT;break;case '[':k=T_LB;break;case ']':k=T_RB;break;case '.':k=T_DOT;break;case ';':k=T_SEMI;break;default:{Token t={0};t.line=line;t.col=col;swan_error(err,&t,"unexpected character '%c'",c);return false;}}
  if(!swan_emit(ts,k,src+i,n,sl,sc,err))return false;
  i+=n;col+=(unsigned)n;
 }
 while(depth>1){depth--;if(!swan_emit(ts,T_DEDENT,"",0,line,col,err))return false;}
 return swan_emit(ts,T_EOF,"",0,line,col,err);
}
#endif
