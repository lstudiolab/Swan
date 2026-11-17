#ifndef SWAN_SIL_GEN_H
#define SWAN_SIL_GEN_H
#include "SILBuilder.h"
static inline uint32_t swan_sil_expr(SwanSILModule *m,SwanSILFunction *f,const Expr *e) {
 if(!e)return 0;
 uint32_t a=swan_sil_expr(m,f,e->left),b=swan_sil_expr(m,f,e->right);
 uint32_t args[3]={0};size_t argc=0;
 if(e->kind==E_CALL){
  if(e->left&&e->left->kind==E_NAME){
   SwanSILInstruction ref={0};ref.opcode=SWAN_SIL_FUNCTION_REF;ref.type=SWAN_SIL_TYPE_FUNCTION;
   ref.ownership=SWAN_SIL_OWNERSHIP_NONE;ref.text=e->left->token.text;ref.location=&e->token;
   args[argc++]=swan_sil_emit(m,f,ref);
  }else if(a)args[argc++]=a;
  for(size_t i=0;i<e->argc&&argc<3;i++){uint32_t v=swan_sil_expr(m,f,e->args[i]);if(v)args[argc++]=v;}
 }
 SwanSILInstruction in={0};in.location=&e->token;
 in.type=(e->kind==E_INT||e->kind==E_FLOAT||e->kind==E_BOOL)?SWAN_SIL_TYPE_TRIVIAL:
         (e->kind==E_STRING?SWAN_SIL_TYPE_OBJECT:
         (e->kind==E_CALL?SWAN_SIL_TYPE_UNKNOWN:
         (e->kind==E_NAME?SWAN_SIL_TYPE_UNKNOWN:SWAN_SIL_TYPE_TRIVIAL)));
 in.ownership=in.type==SWAN_SIL_TYPE_OBJECT?SWAN_SIL_OWNED:SWAN_SIL_OWNERSHIP_NONE;
 switch(e->kind){
  case E_INT:in.opcode=SWAN_SIL_INTEGER_LITERAL;in.integer=e->token.text?strtoll(e->token.text,NULL,10):0;break;
  case E_FLOAT:in.opcode=SWAN_SIL_FLOAT_LITERAL;in.text=e->token.text;break;
  case E_STRING:in.opcode=SWAN_SIL_STRING_LITERAL;in.text=e->token.text;break;
  case E_BOOL:in.opcode=SWAN_SIL_BOOL_LITERAL;in.integer=e->token.kind==T_TRUE;break;
  case E_NAME:in.opcode=SWAN_SIL_DEBUG_VALUE;in.text=e->token.text;in.type=SWAN_SIL_TYPE_UNKNOWN;break;
  case E_CALL:in.opcode=SWAN_SIL_APPLY;for(size_t i=0;i<argc;i++)in.operands[i]=args[i];in.operand_count=argc;in.side_effecting=true;break;
  case E_UNARY:in.opcode=SWAN_SIL_UNARY;if(a){in.operands[0]=a;in.operand_count=1;}in.text=e->token.text;break;
  case E_BINARY:in.opcode=SWAN_SIL_BINARY;if(a&&b){in.operands[0]=a;in.operands[1]=b;in.operand_count=2;}in.text=e->token.text;break;
  case E_INDEX:case E_MEMBER:in.opcode=SWAN_SIL_LOAD;if(a){in.operands[0]=a;in.operand_count=1;}in.side_effecting=true;break;
  default:return 0;
 }
 return swan_sil_emit(m,f,in);
}
static inline bool swan_sil_statements(SwanSILModule *m,SwanSILFunction *f,Stmt **body,size_t count) {
 for(size_t i=0;i<count;i++){
  Stmt *s=body[i];if(!s)continue;
  if(f->blocks[f->current_block].terminated)break;
  if(s->kind==S_VARIABLE){
   SwanSILInstruction alloc={0};alloc.opcode=SWAN_SIL_ALLOC_STACK;alloc.type=SWAN_SIL_TYPE_ADDRESS;
   alloc.text=s->name;alloc.location=&s->token;uint32_t address=swan_sil_emit(m,f,alloc);
   uint32_t value=swan_sil_expr(m,f,s->value);
   if(value){SwanSILInstruction store={0};store.opcode=SWAN_SIL_STORE;store.type=SWAN_SIL_TYPE_TRIVIAL;store.operands[0]=address;store.operands[1]=value;store.operand_count=2;store.side_effecting=true;store.location=&s->token;if(!swan_sil_emit(m,f,store))return false;}
  }else if(s->kind==S_RETURN){
   uint32_t value=swan_sil_expr(m,f,s->value);SwanSILInstruction ret={0};ret.opcode=SWAN_SIL_RETURN;ret.type=SWAN_SIL_TYPE_TRIVIAL;ret.side_effecting=true;ret.terminates_block=true;ret.location=&s->token;if(value){ret.operands[0]=value;ret.operand_count=1;}if(!swan_sil_emit(m,f,ret))return false;
  }else if(s->kind==S_IF){
   uint32_t condition=swan_sil_expr(m,f,s->condition);
   uint32_t then_block=swan_sil_add_block(f),else_block=swan_sil_add_block(f),merge_block=swan_sil_add_block(f);
   if(!condition||then_block==UINT32_MAX||else_block==UINT32_MAX||merge_block==UINT32_MAX||
      !swan_sil_cond_branch(m,f,condition,then_block,else_block,&s->token))return false;
   f->current_block=then_block;
   if(!swan_sil_statements(m,f,s->body,s->body_count))return false;
   if(!f->blocks[f->current_block].terminated&&!swan_sil_branch(m,f,merge_block,&s->token))return false;
   f->current_block=else_block;
   if(!swan_sil_statements(m,f,s->alternate,s->alt_count))return false;
   if(!f->blocks[f->current_block].terminated&&!swan_sil_branch(m,f,merge_block,&s->token))return false;
   f->current_block=merge_block;
  }else if(s->kind==S_WHILE){
   uint32_t condition_block=swan_sil_add_block(f),body_block=swan_sil_add_block(f),exit_block=swan_sil_add_block(f);
   if(condition_block==UINT32_MAX||body_block==UINT32_MAX||exit_block==UINT32_MAX||
      !swan_sil_branch(m,f,condition_block,&s->token))return false;
   f->current_block=condition_block;
   uint32_t condition=swan_sil_expr(m,f,s->condition);
   if(!condition||!swan_sil_cond_branch(m,f,condition,body_block,exit_block,&s->token))return false;
   if(!swan_sil_reserve((void**)&f->loops,&f->loop_capacity,f->loop_count+1,sizeof(*f->loops)))return false;
   f->loops[f->loop_count++]=(SwanSILLoopTarget){exit_block,condition_block};
   f->current_block=body_block;
   if(!swan_sil_statements(m,f,s->body,s->body_count))return false;
   if(!f->blocks[f->current_block].terminated&&!swan_sil_branch(m,f,condition_block,&s->token))return false;
   f->loop_count--;
   f->current_block=exit_block;
  }else if(s->kind==S_LOG){
   uint32_t value=swan_sil_expr(m,f,s->value);SwanSILInstruction log={0};log.opcode=SWAN_SIL_LOG;log.type=SWAN_SIL_TYPE_TRIVIAL;log.side_effecting=true;log.location=&s->token;if(value){log.operands[0]=value;log.operand_count=1;}if(!swan_sil_emit(m,f,log))return false;
  }else if(s->kind==S_EXPR){(void)swan_sil_expr(m,f,s->value);}
  else if(s->kind==S_BREAK||s->kind==S_CONTINUE){
   if(!f->loop_count)return false;
   SwanSILLoopTarget loop=f->loops[f->loop_count-1];
   uint32_t target=s->kind==S_BREAK?loop.break_block:loop.continue_block;
   if(!swan_sil_branch(m,f,target,&s->token))return false;
  }
 }
 return true;
}
static inline bool swan_silgen_program(const Program *p,SwanSILModule *m,SwanError *error) {
 if(!p||!m){swan_error(error,NULL,"SILGen requires a checked program and output module");return false;}
 memset(m,0,sizeof(*m));
 for(size_t i=0;i<p->count;i++){
  const Stmt *ast=p->functions[i];if(!ast||ast->kind!=S_FUNCTION)continue;
  SwanSILFunction *f=swan_sil_add_function(m,ast->link_name?ast->link_name:ast->name,ast->param_count,ast->external);
  if(!f){swan_error(error,&ast->token,"SILGen: out of memory creating function");swan_sil_destroy(m);return false;}
  if(!ast->external&&!swan_sil_statements(m,f,ast->body,ast->body_count)){
   swan_error(error,&ast->token,"SILGen: out of memory lowering function '%s'",ast->name);swan_sil_destroy(m);return false;
  }
 }
 return true;
}

#endif
