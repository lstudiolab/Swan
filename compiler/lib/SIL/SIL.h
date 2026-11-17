#ifndef SWAN_SIL_H
#define SWAN_SIL_H
#include "swan/ast.h"

/*
 * Swan SIL is a typed, ownership-aware, SSA-like high-level IR.
 * Its ownership conventions intentionally follow Swift SIL concepts:
 * owned values transfer destruction responsibility, guaranteed values are
 * borrowed for a lexical scope, and trivial values need no ownership traffic.
 * SIL is compiler IR, never a runtime bytecode format.
 */
typedef enum {
 SWAN_SIL_TYPE_UNKNOWN, SWAN_SIL_TYPE_TRIVIAL, SWAN_SIL_TYPE_OBJECT,
 SWAN_SIL_TYPE_ADDRESS, SWAN_SIL_TYPE_FUNCTION
} SwanSILTypeKind;
typedef enum {
 SWAN_SIL_OWNERSHIP_NONE, SWAN_SIL_OWNED,
 SWAN_SIL_GUARANTEED, SWAN_SIL_UNOWNED
} SwanSILOwnership;
typedef enum {
 SWAN_SIL_INTEGER_LITERAL, SWAN_SIL_FLOAT_LITERAL, SWAN_SIL_STRING_LITERAL,
 SWAN_SIL_BOOL_LITERAL, SWAN_SIL_FUNCTION_REF, SWAN_SIL_APPLY,
 SWAN_SIL_UNARY, SWAN_SIL_BINARY, SWAN_SIL_ALLOC_STACK,
 SWAN_SIL_LOAD, SWAN_SIL_STORE, SWAN_SIL_COPY_VALUE,
 SWAN_SIL_BORROW_VALUE, SWAN_SIL_END_BORROW, SWAN_SIL_DESTROY_VALUE,
 SWAN_SIL_RETURN, SWAN_SIL_BRANCH, SWAN_SIL_COND_BRANCH,
 SWAN_SIL_DEBUG_VALUE, SWAN_SIL_LOG
} SwanSILOpcode;
typedef struct {
 uint32_t id;
 SwanSILOpcode opcode;
 SwanSILTypeKind type;
 SwanSILOwnership ownership;
 uint32_t operands[3];
 size_t operand_count;
 int64_t integer;
 const char *text;
 const Token *location;
 bool side_effecting;
 bool deleted;
} SwanSILInstruction;
typedef struct {
 const char *name;
 SwanSILInstruction *instructions;
 size_t instruction_count, instruction_capacity;
 size_t parameter_count;
 bool external;
} SwanSILFunction;
typedef struct {
 SwanSILFunction *functions;
 size_t function_count, function_capacity;
 size_t instruction_count;
} SwanSILModule;
typedef struct {
 size_t passes_executed, rounds;
 bool converged;
} SwanSILStats;

static inline bool swan_sil_reserve(void **items,size_t *capacity,size_t needed,size_t size) {
 if(!items||!capacity||!size||needed>SIZE_MAX/size)return false;
 if(needed<=*capacity)return true;
 size_t n=*capacity?*capacity:16;
 while(n<needed){if(n>SIZE_MAX/2){n=needed;break;}n*=2;}
 if(n>SIZE_MAX/size)return false;
 void *p=realloc(*items,n*size);if(!p)return false;*items=p;*capacity=n;return true;
}
static inline void swan_sil_destroy(SwanSILModule *m) {
 if(!m)return;
 for(size_t i=0;i<m->function_count;i++)free(m->functions[i].instructions);
 free(m->functions);memset(m,0,sizeof(*m));
}
static inline SwanSILFunction *swan_sil_add_function(SwanSILModule *m,const char *name,size_t params,bool external) {
 if(!m||!name||!swan_sil_reserve((void**)&m->functions,&m->function_capacity,m->function_count+1,sizeof(*m->functions)))return NULL;
 SwanSILFunction *f=&m->functions[m->function_count++];memset(f,0,sizeof(*f));
 f->name=name;f->parameter_count=params;f->external=external;return f;
}
static inline uint32_t swan_sil_emit(SwanSILModule *m,SwanSILFunction *f,SwanSILInstruction in) {
 if(!m||!f||f->instruction_count>=UINT32_MAX||
    !swan_sil_reserve((void**)&f->instructions,&f->instruction_capacity,f->instruction_count+1,sizeof(*f->instructions)))return 0;
 in.id=(uint32_t)f->instruction_count+1;
 f->instructions[f->instruction_count++]=in;m->instruction_count++;return in.id;
}
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
  if(s->kind==S_VARIABLE){
   SwanSILInstruction alloc={0};alloc.opcode=SWAN_SIL_ALLOC_STACK;alloc.type=SWAN_SIL_TYPE_ADDRESS;
   alloc.text=s->name;alloc.location=&s->token;uint32_t address=swan_sil_emit(m,f,alloc);
   uint32_t value=swan_sil_expr(m,f,s->value);
   if(value){SwanSILInstruction store={0};store.opcode=SWAN_SIL_STORE;store.type=SWAN_SIL_TYPE_TRIVIAL;store.operands[0]=address;store.operands[1]=value;store.operand_count=2;store.side_effecting=true;store.location=&s->token;if(!swan_sil_emit(m,f,store))return false;}
  }else if(s->kind==S_RETURN){
   uint32_t value=swan_sil_expr(m,f,s->value);SwanSILInstruction ret={0};ret.opcode=SWAN_SIL_RETURN;ret.type=SWAN_SIL_TYPE_TRIVIAL;ret.side_effecting=true;ret.location=&s->token;if(value){ret.operands[0]=value;ret.operand_count=1;}if(!swan_sil_emit(m,f,ret))return false;
  }else if(s->kind==S_IF||s->kind==S_WHILE){
   uint32_t cond=swan_sil_expr(m,f,s->condition);SwanSILInstruction branch={0};branch.opcode=SWAN_SIL_COND_BRANCH;branch.type=SWAN_SIL_TYPE_TRIVIAL;branch.side_effecting=true;branch.location=&s->token;if(cond){branch.operands[0]=cond;branch.operand_count=1;}if(!swan_sil_emit(m,f,branch))return false;
   if(!swan_sil_statements(m,f,s->body,s->body_count)||!swan_sil_statements(m,f,s->alternate,s->alt_count))return false;
  }else if(s->kind==S_LOG){
   uint32_t value=swan_sil_expr(m,f,s->value);SwanSILInstruction log={0};log.opcode=SWAN_SIL_LOG;log.type=SWAN_SIL_TYPE_TRIVIAL;log.side_effecting=true;log.location=&s->token;if(value){log.operands[0]=value;log.operand_count=1;}if(!swan_sil_emit(m,f,log))return false;
  }else if(s->kind==S_EXPR){(void)swan_sil_expr(m,f,s->value);}
  else if(s->kind==S_PASS||s->kind==S_BREAK||s->kind==S_CONTINUE){
   SwanSILInstruction marker={0};marker.opcode=SWAN_SIL_BRANCH;marker.type=SWAN_SIL_TYPE_TRIVIAL;marker.side_effecting=true;marker.location=&s->token;if(!swan_sil_emit(m,f,marker))return false;
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
static inline bool swan_sil_verify(const SwanSILModule *m,char *error,size_t cap) {
 if(!m){if(error&&cap)snprintf(error,cap,"null SIL module");return false;}
 for(size_t fi=0;fi<m->function_count;fi++){
  const SwanSILFunction *f=&m->functions[fi];
  for(size_t i=0;i<f->instruction_count;i++){
   const SwanSILInstruction *in=&f->instructions[i];
   if(in->id!=i+1||in->operand_count>3){if(error&&cap)snprintf(error,cap,"malformed SIL instruction in %s at %zu",f->name,i);return false;}
   for(size_t k=0;k<in->operand_count;k++)if(!in->operands[k]||in->operands[k]>=in->id){
    if(error&&cap)snprintf(error,cap,"SIL use-before-definition in %s at instruction %zu",f->name,i+1);return false;
   }
   if(in->ownership==SWAN_SIL_GUARANTEED&&in->type!=SWAN_SIL_TYPE_OBJECT){
    if(error&&cap)snprintf(error,cap,"guaranteed ownership applied to non-object SIL value in %s",f->name);return false;
   }
   if(in->opcode==SWAN_SIL_STORE&&in->operand_count!=2){
    if(error&&cap)snprintf(error,cap,"SIL store requires address and value in %s",f->name);return false;
   }
  }
 }
 return true;
}
static inline bool swan_sil_canonicalize(SwanSILModule *m,bool *changed) {
 bool any=false;
 for(size_t f=0;f<m->function_count;f++)for(size_t i=0;i<m->functions[f].instruction_count;i++){
  SwanSILInstruction *in=&m->functions[f].instructions[i];
  if(in->opcode==SWAN_SIL_COPY_VALUE&&in->operand_count==1){in->deleted=true;any=true;}
  if(in->opcode==SWAN_SIL_BORROW_VALUE&&in->ownership!=SWAN_SIL_GUARANTEED){in->ownership=SWAN_SIL_GUARANTEED;any=true;}
 }
 if(changed)*changed=any;return true;
}
static inline bool swan_sil_dead_value_elimination(SwanSILModule *m,bool *changed) {
 bool any=false;
 for(size_t f=0;f<m->function_count;f++){
  SwanSILFunction *fn=&m->functions[f];bool again=true;
  while(again){again=false;
   for(size_t i=0;i<fn->instruction_count;i++){
    SwanSILInstruction *in=&fn->instructions[i];if(in->deleted||in->side_effecting||in->opcode==SWAN_SIL_RETURN||in->opcode==SWAN_SIL_BRANCH||in->opcode==SWAN_SIL_COND_BRANCH)continue;
    bool used=false;for(size_t j=0;j<fn->instruction_count&&!used;j++)if(!fn->instructions[j].deleted)
     for(size_t k=0;k<fn->instructions[j].operand_count;k++)if(fn->instructions[j].operands[k]==in->id){used=true;break;}
    if(!used){in->deleted=true;again=any=true;}
   }
  }
 }
 if(changed)*changed=any;return true;
}
static inline bool swan_sil_ownership_simplify(SwanSILModule *m,bool *changed) {
 bool any=false;
 for(size_t f=0;f<m->function_count;f++)for(size_t i=0;i<m->functions[f].instruction_count;i++){
  SwanSILInstruction *in=&m->functions[f].instructions[i];
  if(in->type==SWAN_SIL_TYPE_TRIVIAL&&in->ownership!=SWAN_SIL_OWNERSHIP_NONE){in->ownership=SWAN_SIL_OWNERSHIP_NONE;any=true;}
 }
 if(changed)*changed=any;return true;
}
static inline bool swan_sil_optimize(SwanSILModule *m,SwanSILStats *stats,char *error,size_t cap) {
 static const char *names[]={"sil-canonicalization","sil-ownership-simplification","sil-dead-value-elimination"};
 (void)names;
 if(stats)memset(stats,0,sizeof(*stats));
 if(!swan_sil_verify(m,error,cap))return false;
 bool converged=false;size_t executed=0,rounds=0;
 while(executed<90){
  bool round_changed=false;rounds++;
  for(unsigned pass=0;pass<3&&executed<90;pass++,executed++){
   bool changed=false,ok=pass==0?swan_sil_canonicalize(m,&changed):
      pass==1?swan_sil_ownership_simplify(m,&changed):swan_sil_dead_value_elimination(m,&changed);
   if(!ok){if(error&&cap)snprintf(error,cap,"SIL optimization pass failed: %s",names[pass]);return false;}
   round_changed|=changed;
  }
  if(!swan_sil_verify(m,error,cap))return false;
  if(!round_changed){converged=true;break;}
 }
 if(stats){stats->passes_executed=executed;stats->rounds=rounds;stats->converged=converged;}
 if(!converged){if(error&&cap)snprintf(error,cap,"SIL optimization did not converge within 90 pass executions");return false;}
 return swan_sil_verify(m,error,cap);
}
static inline bool swan_sil_prepare_program(const Program *program,SwanError *error) {
 SwanSILModule module={0};SwanSILStats stats={0};char detail[256]={0};
 bool ok=swan_silgen_program(program,&module,error);
 if(ok)ok=swan_sil_optimize(&module,&stats,detail,sizeof(detail));
 if(!ok&&error&&!error->message[0])swan_error(error,NULL,"SIL pipeline failed: %s",detail);
 swan_sil_destroy(&module);return ok;
}
#endif
