#ifndef SWAN_LEXER_H
#define SWAN_LEXER_H
#include "swan/core.h"
static inline TokenKind swan_keyword(const char *s) {
 if(!strcmp(s,"fn"))return T_FN;if(!strcmp(s,"let"))return T_LET;if(!strcmp(s,"var"))return T_VAR;
 if(!strcmp(s,"const"))return T_CONST;if(!strcmp(s,"return"))return T_RETURN;if(!strcmp(s,"log"))return T_LOG;
 if(!strcmp(s,"if"))return T_IF;if(!strcmp(s,"else"))return T_ELSE;if(!strcmp(s,"while"))return T_WHILE;
 if(!strcmp(s,"break"))return T_BREAK;if(!strcmp(s,"continue"))return T_CONTINUE;if(!strcmp(s,"pass"))return T_PASS;
 if(!strcmp(s,"true"))return T_TRUE;if(!strcmp(s,"false"))return T_FALSE;if(!strcmp(s,"none"))return T_NONE;
 if(!strcmp(s,"and"))return T_AND;if(!strcmp(s,"or"))return T_OR;if(!strcmp(s,"not"))return T_NOT;return T_IDENT;
}
static inline bool swan_emit(TokenStream *ts,TokenKind k,const char *s,size_t n,unsigned line,unsigned col,SwanError *err) {
 if(ts->count>=SWAN_MAX_TOKENS){Token t={0};swan_error(err,&t,"source has too many tokens");return false;}
 Token *t=&ts->items[ts->count++];t->kind=k;t->line=line;t->col=col;
 if(n>=sizeof(t->text)){swan_error(err,t,"token is too long");return false;}memcpy(t->text,s,n);t->text[n]=0;return true;
}
static inline bool swan_lex(const char *src,TokenStream *ts,SwanError *err) {
 memset(ts,0,sizeof(*ts));ts->source=src;ts->length=strlen(src);
 size_t i=0;unsigned line=1,col=1;unsigned indents[128]={0};size_t depth=1;bool line_start=true;
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
  if(isalpha((unsigned char)c)||c=='_'){while(i<ts->length&&(isalnum((unsigned char)src[i])||src[i]=='_')){i++;col++;}size_t n=i-st;char word[SWAN_MAX_TEXT];if(n>=sizeof(word)){Token t={0};t.line=sl;t.col=sc;swan_error(err,&t,"identifier too long");return false;}memcpy(word,src+st,n);word[n]=0;if(!swan_emit(ts,swan_keyword(word),word,n,sl,sc,err))return false;continue;}
  if(isdigit((unsigned char)c)){while(i<ts->length&&isdigit((unsigned char)src[i])){i++;col++;}TokenKind k=T_INT;if(i+1<ts->length&&src[i]=='.'&&isdigit((unsigned char)src[i+1])){k=T_FLOAT;i++;col++;while(i<ts->length&&isdigit((unsigned char)src[i])){i++;col++;}}if(!swan_emit(ts,k,src+st,i-st,sl,sc,err))return false;continue;}
  if(c=='"'||c=='\''){char quote=c;i++;col++;char value[SWAN_MAX_TEXT];size_t n=0;bool closed=false;while(i<ts->length){c=src[i++];col++;if(c==quote){closed=true;break;}if(c=='\\'){if(i>=ts->length)break;c=src[i++];col++;switch(c){case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;case '\\':case '"':case '\'':break;default:{Token t={0};t.line=sl;t.col=sc;swan_error(err,&t,"unknown string escape");return false;}}}if(n+1>=sizeof(value)){Token t={0};t.line=sl;t.col=sc;swan_error(err,&t,"string literal too long");return false;}value[n++]=c;}if(!closed){Token t={0};t.line=sl;t.col=sc;swan_error(err,&t,"unterminated string");return false;}if(!swan_emit(ts,T_STRING,value,n,sl,sc,err))return false;continue;}
  TokenKind k=T_EOF;size_t n=1;
  if(i+1<ts->length){char d=src[i+1];if(c=='-'&&d=='>'){k=T_ARROW;n=2;}else if(c=='/'&&d=='/'){k=T_FLOORDIV;n=2;}else if(c=='='&&d=='='){k=T_EQ;n=2;}else if(c=='!'&&d=='='){k=T_NE;n=2;}else if(c=='<'&&d=='='){k=T_LE;n=2;}else if(c=='>'&&d=='='){k=T_GE;n=2;}}
  if(k==T_EOF)switch(c){case '(':k=T_LP;break;case ')':k=T_RP;break;case ':':k=T_COLON;break;case ',':k=T_COMMA;break;case '+':k=T_PLUS;break;case '-':k=T_MINUS;break;case '*':k=T_STAR;break;case '/':k=T_SLASH;break;case '%':k=T_PERCENT;break;case '=':k=T_ASSIGN;break;case '<':k=T_LT;break;case '>':k=T_GT;break;case '[':k=T_LB;break;case ']':k=T_RB;break;case '.':k=T_DOT;break;case ';':k=T_SEMI;break;default:{Token t={0};t.line=line;t.col=col;swan_error(err,&t,"unexpected character '%c'",c);return false;}}
  if(!swan_emit(ts,k,src+i,n,sl,sc,err))return false;i+=n;col+=(unsigned)n;
 }
 while(depth>1){depth--;if(!swan_emit(ts,T_DEDENT,"",0,line,col,err))return false;}
 return swan_emit(ts,T_EOF,"",0,line,col,err);
}
#endif
