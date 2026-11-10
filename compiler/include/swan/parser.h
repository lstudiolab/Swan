#ifndef SWAN_PARSER_H
#define SWAN_PARSER_H
#include "swan/lexer.h"
#include "swan/ast.h"
typedef struct { TokenStream *ts; Program *program; SwanError *err; bool failed; } Parser;
static inline Token *pk(Parser *p){return &p->ts->items[p->ts->pos<p->ts->count?p->ts->pos:p->ts->count-1];}
static inline bool is(Parser *p,TokenKind k){return pk(p)->kind==k;}
static inline Token take(Parser *p){Token t=*pk(p);if(t.kind!=T_EOF)p->ts->pos++;return t;}
static inline bool eat(Parser *p,TokenKind k){if(!is(p,k))return false;take(p);return true;}
static inline bool need(Parser *p,TokenKind k,const char *message){if(eat(p,k))return true;swan_error(p->err,pk(p),"%s",message);p->failed=true;return false;}
static inline void separators(Parser *p){while(eat(p,T_NEWLINE)||eat(p,T_SEMI)){}}
static inline Expr *new_expr(Parser *p,ExprKind k,Token t){Expr *e=swan_arena_alloc(p->program,sizeof(*e));if(!e){swan_error(p->err,&t,"out of memory allocating AST expression");p->failed=true;return NULL;}e->kind=k;e->token=t;return e;}
static inline Stmt *new_stmt(Parser *p,StmtKind k,Token t){Stmt *s=swan_arena_alloc(p->program,sizeof(*s));if(!s){swan_error(p->err,&t,"out of memory allocating AST statement");p->failed=true;return NULL;}s->kind=k;s->token=t;return s;}
static inline bool swan_type_token(const Token *t){return t&&(t->kind==T_IDENT||t->kind==T_BOOL_TYPE||(t->kind==T_INT&&t->text&&!strcmp(t->text,"int")));}
static inline int precedence(TokenKind k){switch(k){case T_ASSIGN:return 1;case T_OR:return 2;case T_AND:return 3;case T_EQ:case T_NE:return 4;case T_LT:case T_GT:case T_LE:case T_GE:return 5;case T_PLUS:case T_MINUS:return 6;case T_STAR:case T_SLASH:case T_FLOORDIV:case T_PERCENT:return 7;default:return 0;}}
static inline Expr *expression(Parser *p,int min);
static inline Expr *prefix(Parser *p){
 Token t=take(p);Expr *e=NULL;
 if(t.kind==T_INT||t.kind==T_FLOAT||t.kind==T_STRING||t.kind==T_TRUE||t.kind==T_FALSE||t.kind==T_NONE){e=new_expr(p,t.kind==T_INT?E_INT:t.kind==T_FLOAT?E_FLOAT:t.kind==T_STRING?E_STRING:E_BOOL,t);}
 else if(t.kind==T_MINUS||t.kind==T_NOT||(t.kind==T_IDENT&&!strcmp(t.text,"not"))){if(t.kind==T_IDENT)t.kind=T_NOT;e=new_expr(p,E_UNARY,t);if(e){e->left=expression(p,8);e->contains_call=e->left&&e->left->contains_call;}}
 else if(t.kind==T_IDENT||t.kind==T_FREE||t.kind==T_SELF){e=new_expr(p,E_NAME,t);}
 else if(t.kind==T_LP){e=expression(p,1);need(p,T_RP,"expected ')'");}
 else{swan_error(p->err,&t,"expected expression, found token %d '%s'",(int)t.kind,t.text?t.text:"");p->failed=true;return NULL;}
 while(e&&!p->failed){
  if(eat(p,T_LB)){
   Expr *index=expression(p,1);
   need(p,T_RB,"expected ']' after array index");
   Expr *item=new_expr(p,E_INDEX,e->token);
   if(item){item->left=e;item->right=index;item->contains_call=(e&&e->contains_call)||(index&&index->contains_call);}
   e=item;
  }
  else if(eat(p,T_LP)){Expr *call=new_expr(p,E_CALL,e->token);if(!call)return e;call->left=e;call->contains_call=true;if(!is(p,T_RP)){call->args=swan_arena_alloc(p->program,16*sizeof(*call->args));if(!call->args){swan_error(p->err,&call->token,"out of memory allocating call arguments");p->failed=true;return e;}do{if(call->argc>=16){swan_error(p->err,pk(p),"too many call arguments");p->failed=true;break;}call->args[call->argc]=expression(p,1);if(call->args[call->argc])call->contains_call|=call->args[call->argc]->contains_call;call->argc++;}while(eat(p,T_COMMA));}need(p,T_RP,"expected ')' after arguments");e=call;}
  else if(eat(p,T_DOT)){Token member=take(p);if(!member.text||member.kind==T_EOF||member.kind==T_NEWLINE||member.kind==T_DOT){swan_error(p->err,&member,"expected a member name after '.'");p->failed=true;break;}Expr *field=new_expr(p,E_NAME,member);Expr *access=new_expr(p,E_MEMBER,e->token);if(access){access->left=e;access->right=field;access->contains_call=e->contains_call;}e=access;}
  else break;
 }
 return e;
}
static inline int precedence_token(Token *t){if(t->kind==T_IDENT){if(!strcmp(t->text,"or"))return 2;if(!strcmp(t->text,"and"))return 3;}return precedence(t->kind);}
static inline Expr *expression(Parser *p,int min){Expr *left=prefix(p);while(left&&!p->failed){int prec=precedence_token(pk(p));if(prec<min)break;Token op=take(p);if(op.kind==T_IDENT){if(!strcmp(op.text,"or"))op.kind=T_OR;else if(!strcmp(op.text,"and"))op.kind=T_AND;}Expr *right=expression(p,op.kind==T_ASSIGN?prec:prec+1);Expr *e=new_expr(p,E_BINARY,op);if(!e)return left;e->left=left;e->right=right;e->contains_call=(left&&left->contains_call)||(right&&right->contains_call);left=e;}return left;}
static inline bool block(Parser *p,Stmt ***out,size_t *count,size_t *capacity);
static inline Stmt *statement(Parser *p){
 Token t=*pk(p);Stmt *s=NULL;bool manual=eat(p,T_MANUAL);bool legacy_store=false;if(!manual){legacy_store=eat(p,T_STORE);manual=legacy_store;}if(manual)t=*pk(p);
 if(manual&&!is(p,T_VAR)&&!(legacy_store&&is(p,T_LET))){swan_error(p->err,pk(p),"manual must modify a mutable var declaration");p->failed=true;return NULL;}
 if(eat(p,T_LET)||eat(p,T_VAR)||eat(p,T_CONST)){bool mut=t.kind==T_VAR||manual;s=new_stmt(p,S_VARIABLE,t);if(!s)return NULL;s->mutable_value=mut;s->manual_memory=manual;s->manual_checked=manual&&!legacy_store;Token n=take(p);if(n.kind!=T_IDENT){swan_error(p->err,&n,"expected variable name");p->failed=true;return s;} s->name=n.text;if(eat(p,T_COLON)){Token ty=take(p);if(!swan_type_token(&ty)){swan_error(p->err,&ty,"expected type name");p->failed=true;}else s->type=ty.text;}if(eat(p,T_ASSIGN))s->value=expression(p,1);if(!mut&&!s->value){swan_error(p->err,&t,"immutable declaration requires a value");p->failed=true;}return s;}
 bool print_intrinsic=eat(p,T_PRINT);if(print_intrinsic||(is(p,T_IDENT)&&(!strcmp(pk(p)->text,"print")||!strcmp(pk(p)->text,"log")))){if(!print_intrinsic){take(p);if(eat(p,T_GT)&&!need(p,T_GT,"expected second '>' in log output operator"))return NULL;}s=new_stmt(p,S_LOG,t);if(s)s->value=expression(p,1);return s;}
 if(is(p,T_IDENT)&&!strcmp(pk(p)->text,"echo")&&p->ts->pos+1<p->ts->count&&p->ts->items[p->ts->pos+1].kind==T_BANG){take(p);take(p);s=new_stmt(p,S_LOG,t);if(s)s->value=expression(p,1);return s;}
 if(eat(p,T_RETURN)){s=new_stmt(p,S_RETURN,t);if(s&&!is(p,T_NEWLINE)&&!is(p,T_DEDENT)&&!is(p,T_EOF))s->value=expression(p,1);return s;}
 if(eat(p,T_BREAK))return new_stmt(p,S_BREAK,t);
 if(eat(p,T_CONTINUE))return new_stmt(p,S_CONTINUE,t);
 if(eat(p,T_PASS))return new_stmt(p,S_PASS,t);
 if(eat(p,T_IF)||eat(p,T_WHILE)){bool cond=t.kind==T_IF;s=new_stmt(p,cond?S_IF:S_WHILE,t);s->condition=expression(p,1);if(!block(p,&s->body,&s->body_count,&s->body_cap))return s;if(cond){separators(p);}if(cond&&eat(p,T_ELSE)){if(is(p,T_IF)){ s->alternate=swan_arena_alloc(p->program,sizeof(*s->alternate));if(!s->alternate){swan_error(p->err,&t,"out of memory allocating else branch");p->failed=true;return s;}s->alt_cap=1;s->alternate[0]=statement(p);s->alt_count=1;}else block(p,&s->alternate,&s->alt_count,&s->alt_cap);}return s;}
 if(is(p,T_IDENT)&&!strcmp(pk(p)->text,"free")){
  take(p);Expr *callee=new_expr(p,E_NAME,t);
  if(!need(p,T_LP,"expected '(' after free"))return NULL;
  Expr *argument=expression(p,1);
  if(!need(p,T_RP,"expected ')' after free argument"))return NULL;
  Expr *call=new_expr(p,E_CALL,t);if(!call)return NULL;
  call->left=callee;call->args=swan_arena_alloc(p->program,sizeof(*call->args));
  if(!call->args){swan_error(p->err,&t,"out of memory parsing free");p->failed=true;return NULL;}
  call->args[0]=argument;call->argc=1;call->contains_call=true;
  s=new_stmt(p,S_FREE,t);if(s)s->value=call;return s;
 }
 s=new_stmt(p,S_EXPR,t);if(s)s->value=expression(p,1);return s;
}
static inline bool block(Parser *p,Stmt ***out,size_t *count,size_t *capacity){
 bool colon=eat(p,T_COLON);
 if(!need(p,T_NEWLINE,"expected newline"))return false;
 if(colon){if(!need(p,T_INDENT,"expected indentation after ':'"))return false;}
 else if(!need(p,T_INDENT,"expected indentation"))return false;
 separators(p);
 while(!is(p,T_DEDENT)&&!is(p,T_EOF)&&!p->failed){
  if(*count>=1024){swan_error(p->err,pk(p),"block too large");p->failed=true;break;}
  if(*count==*capacity){
   size_t cap=*capacity?*capacity*2:8;if(cap>1024)cap=1024;
   Stmt **items=swan_arena_alloc(p->program,cap*sizeof(*items));
   if(!items){swan_error(p->err,pk(p),"out of memory while parsing block");p->failed=true;break;}
   if(*count)memcpy(items,*out,*count*sizeof(*items));*out=items;*capacity=cap;
  }
  size_t before=p->ts->pos;(*out)[(*count)++]=statement(p);separators(p);
  if(before==p->ts->pos){swan_error(p->err,pk(p),"parser made no progress");p->failed=true;}
 }
 if(p->failed)return false;
 if(!need(p,T_DEDENT,"expected dedent to close block"))return false;
 /* 'out' remains accepted as a deprecated terminator for older Swan files. */
 eat(p,T_OUT);
 return true;
}
static inline bool swan_parse(TokenStream *ts,Program *program,SwanError *err){Parser p={ts,program,err,false};separators(&p);while(!is(&p,T_EOF)&&!p.failed){bool lazy_import=eat(&p,T_LAZY);Token t=take(&p);if(lazy_import&&t.kind!=T_IMPORT){swan_error(err,&t,"lazy must modify an import declaration");return false;}if(t.kind==T_IMPORT){Token module=take(&p);if(module.kind!=T_IDENT){swan_error(err,&module,"expected module name after import");return false;}if(!strcmp(module.text,"C")){Token header=take(&p);if(header.kind!=T_STRING){swan_error(err,&header,"expected C header path after import C");return false;}if(program->c_header_count>=64){swan_error(err,&header,"too many C header imports");return false;}program->c_headers[program->c_header_count++]=header.text;}else{if(program->import_count>=64){swan_error(err,&module,"too many standard-library imports");return false;}program->imports[program->import_count++]=module.text;}eat(&p,T_SEMI);separators(&p);continue;}bool external=t.kind==T_EXTERN;if(external){t=take(&p);if(t.kind!=T_FN){swan_error(err,&t,"expected func after external");return false;}}if(t.kind!=T_FN){swan_error(err,&t,"only func declarations, external declarations, and imports are currently supported at file scope");return false;}if(program->count>=1024){swan_error(err,&t,"too many functions");return false;}Stmt *fn=new_stmt(&p,S_FUNCTION,t);if(!fn)return false;Token name=take(&p);if(name.kind!=T_IDENT){swan_error(err,&name,"expected function name");return false;}fn->name=name.text;fn->external=external;need(&p,T_LP,"expected '('");if(!is(&p,T_RP)){fn->params=swan_arena_alloc(program,16*sizeof(*fn->params));fn->param_types=swan_arena_alloc(program,16*sizeof(*fn->param_types));if(!fn->params||!fn->param_types){swan_error(err,&t,"out of memory allocating function parameters");return false;}do{Token a=take(&p);if(a.kind!=T_IDENT||fn->param_count>=16){swan_error(err,&a,"invalid or excessive parameter list");return false;}fn->params[fn->param_count]=a.text;fn->param_types[fn->param_count]=NULL;fn->param_count++;if(eat(&p,T_COLON)){Token ty=take(&p);if(!swan_type_token(&ty)){swan_error(err,&ty,"expected parameter type");return false;}fn->param_types[fn->param_count-1]=ty.text;}}while(eat(&p,T_COMMA));}need(&p,T_RP,"expected ')'");if(eat(&p,T_ARROW)){Token ret=take(&p);if(!swan_type_token(&ret)){swan_error(err,&ret,"expected return type");return false;}fn->type=ret.text;}if(external){if(!is(&p,T_NEWLINE)&&!is(&p,T_EOF)){swan_error(err,pk(&p),"external function declarations cannot have a body");return false;}separators(&p);}else if(!block(&p,&fn->body,&fn->body_count,&fn->body_cap))return false;program->functions[program->count++]=fn;separators(&p);}return !p.failed;}
#endif
