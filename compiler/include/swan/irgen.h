#ifndef SWAN_IRGEN_H
#define SWAN_IRGEN_H
#include "swan/ir.h"
#include "swan/ast.h"
#include <errno.h>
#include <limits.h>

typedef struct {
    const char *name;
    uint32_t value;
    SwanIRType type;
    bool memory;
} SwanIRLocal;

typedef struct {
    SwanIRModule *module;
    SwanIRFunction *function;
    size_t block;
    SwanIRLocal locals[256];
    size_t local_count;
    size_t break_targets[64], continue_targets[64];
    size_t loop_depth;
    SwanError *error;
} SwanIRGen;

static inline SwanIRType swan_irgen_type(const char *name) {
    if (!name || !strcmp(name,"int") || !strcmp(name,"i64")) return SWAN_IR_I64;
    if (!strcmp(name,"bool") || !strcmp(name,"i1")) return SWAN_IR_I1;
    if (!strcmp(name,"i8")) return SWAN_IR_I8;
    if (!strcmp(name,"i16")) return SWAN_IR_I16;
    if (!strcmp(name,"i32")) return SWAN_IR_I32;
    if (!strcmp(name,"f32")) return SWAN_IR_F32;
    if (!strcmp(name,"f64") || !strcmp(name,"float")) return SWAN_IR_F64;
    if (!strcmp(name,"void") || !strcmp(name,"none")) return SWAN_IR_VOID;
    return SWAN_IR_VOID;
}
static inline SwanIRBlock *swan_irgen_block(SwanIRGen *g) {
    return g->block < g->function->block_count ? &g->function->blocks[g->block] : NULL;
}
static inline bool swan_irgen_open(SwanIRGen *g) {
    SwanIRBlock *b=swan_irgen_block(g); return b && !b->terminated;
}
static inline bool swan_irgen_fail(SwanIRGen *g,const Token *token,const char *message) {
    swan_error(g->error,token,"%s",message);return false;
}
static inline uint32_t swan_irgen_emit(SwanIRGen *g,SwanIRInst in) {
    uint32_t id=swan_ir_add_inst(g->function,swan_irgen_block(g),in);
    if(!id && in.type!=SWAN_IR_VOID) swan_irgen_fail(g,NULL,"unable to append typed IR instruction");
    return id;
}
static inline uint32_t swan_irgen_const(SwanIRGen *g,SwanIRType type,int64_t value) {
    SwanIRInst in={0};in.op=SWAN_IR_CONST;in.type=type;in.immediate=value;return swan_irgen_emit(g,in);
}
static inline uint32_t swan_irgen_compare_zero(SwanIRGen *g,uint32_t value) {
    SwanIRType t=swan_ir_value_type_of(g->function,value);
    if(t==SWAN_IR_I1)return value;
    uint32_t zero=swan_irgen_const(g,t,0);SwanIRInst in={0};
    in.op=SWAN_IR_ICMP_NE;in.type=SWAN_IR_I1;in.a=value;in.b=zero;return swan_irgen_emit(g,in);
}
static inline SwanIRLocal *swan_irgen_local(SwanIRGen *g,const char *name) {
    for(size_t i=g->local_count;i>0;i--)if(!strcmp(g->locals[i-1].name,name))return &g->locals[i-1];
    return NULL;
}
static inline uint32_t swan_irgen_expr(SwanIRGen *g,Expr *e);
static inline bool swan_irgen_integer_literal(Expr *e,int64_t *value){
    if(!e)return false;
    bool negative=false;
    if(e->kind==E_UNARY&&e->token.kind==T_MINUS&&e->left&&e->left->kind==E_INT){negative=true;e=e->left;}
    if(e->kind!=E_INT)return false;
    errno=0;char *end=NULL;unsigned long long magnitude=strtoull(e->token.text,&end,10);
    if(errno==ERANGE||!end||end==e->token.text||*end)return false;
    if(negative){
        if(magnitude>(unsigned long long)INT64_MAX+1u)return false;
        *value=magnitude==(unsigned long long)INT64_MAX+1u?INT64_MIN:-(int64_t)magnitude;
    }else{
        if(magnitude>(unsigned long long)INT64_MAX)return false;
        *value=(int64_t)magnitude;
    }
    return true;
}

static inline SwanIRType swan_irgen_infer_expr_type(SwanIRGen *g,Expr *e){
    if(!e)return SWAN_IR_VOID;
    if(e->kind==E_BOOL)return SWAN_IR_I1;
    if(e->kind==E_INT)return SWAN_IR_I64;
    if(e->kind==E_NAME){SwanIRLocal *local=swan_irgen_local(g,e->token.text);return local?local->type:SWAN_IR_VOID;}
    if(e->kind==E_CALL&&e->left&&e->left->kind==E_NAME){
        for(size_t i=0;i<g->module->function_count;i++)if(!strcmp(g->module->functions[i].name,e->left->token.text))return g->module->functions[i].return_type;
    }
    if(e->kind==E_UNARY)return e->token.kind==T_NOT?SWAN_IR_I1:swan_irgen_infer_expr_type(g,e->left);
    if(e->kind==E_BINARY){
        if(e->token.kind==T_EQ||e->token.kind==T_NE||e->token.kind==T_LT||e->token.kind==T_LE||e->token.kind==T_GT||e->token.kind==T_GE||e->token.kind==T_AND||e->token.kind==T_OR)return SWAN_IR_I1;
        SwanIRType left=swan_irgen_infer_expr_type(g,e->left);
        return e->left&&e->left->kind==E_INT?swan_irgen_infer_expr_type(g,e->right):left;
    }
    return SWAN_IR_VOID;
}
static inline uint32_t swan_irgen_expr_as(SwanIRGen *g,Expr *e,SwanIRType expected){
    int64_t literal;
    if(swan_irgen_integer_literal(e,&literal)){
        unsigned bits=expected==SWAN_IR_I1?1:expected==SWAN_IR_I8?8:expected==SWAN_IR_I16?16:expected==SWAN_IR_I32?32:expected==SWAN_IR_I64?64:0;
        if(!bits){swan_irgen_fail(g,&e->token,"integer literal cannot initialize this type");return 0;}
        int64_t min=bits==64?INT64_MIN:-(INT64_C(1)<<(bits-1));
        int64_t max=bits==64?INT64_MAX:(INT64_C(1)<<(bits-1))-1;
        if(literal<min||literal>max){swan_irgen_fail(g,&e->token,"integer literal is outside the destination type range");return 0;}
        return swan_irgen_const(g,expected,literal);
    }
    uint32_t value=swan_irgen_expr(g,e);if(!value)return 0;
    if(swan_ir_value_type_of(g->function,value)!=expected){swan_irgen_fail(g,&e->token,"expression type does not match its context");return 0;}
    return value;
}
static inline uint32_t swan_irgen_expr(SwanIRGen *g,Expr *e) {
    if(!e)return 0;
    if(e->kind==E_INT||e->kind==E_BOOL) {
        char *end=NULL;errno=0;int64_t v=e->kind==E_BOOL?(e->token.kind==T_TRUE):strtoll(e->token.text,&end,10);
        if(e->kind==E_INT&&(errno==ERANGE||!end||end==e->token.text||*end)){swan_irgen_fail(g,&e->token,"integer literal is outside the supported IR range");return 0;}
        return swan_irgen_const(g,e->kind==E_BOOL?SWAN_IR_I1:SWAN_IR_I64,v);
    }
    if(e->kind==E_NAME) {
        SwanIRLocal *local=swan_irgen_local(g,e->token.text);
        if(!local){swan_irgen_fail(g,&e->token,"IR generation cannot resolve local name");return 0;}
        if(!local->memory)return local->value;
        SwanIRInst in={0};in.op=SWAN_IR_LOAD;in.type=local->type;in.a=local->value;return swan_irgen_emit(g,in);
    }
    if(e->kind==E_UNARY) {
        uint32_t a=swan_irgen_expr(g,e->left);if(!a)return 0;
        SwanIRType t=swan_ir_value_type_of(g->function,a);
        if(e->token.kind==T_MINUS){uint32_t z=swan_irgen_const(g,t,0);SwanIRInst in={0};in.op=SWAN_IR_SUB;in.type=t;in.a=z;in.b=a;return swan_irgen_emit(g,in);}
        if(e->token.kind==T_NOT){uint32_t z=swan_irgen_const(g,t,0);SwanIRInst in={0};in.op=SWAN_IR_ICMP_EQ;in.type=SWAN_IR_I1;in.a=a;in.b=z;return swan_irgen_emit(g,in);}
        swan_irgen_fail(g,&e->token,"unsupported unary operator");return 0;
    }
    if(e->kind==E_CALL) {
        if(!e->left||e->left->kind!=E_NAME){swan_irgen_fail(g,&e->token,"only direct function calls are supported");return 0;}
        size_t index=g->module->function_count;
        for(size_t i=0;i<g->module->function_count;i++)if(!strcmp(g->module->functions[i].name,e->left->token.text)){index=i;break;}
        if(index==g->module->function_count){swan_irgen_fail(g,&e->token,"unknown function in IR call");return 0;}
        SwanIRFunction *callee=&g->module->functions[index];
        if(callee->parameter_count!=e->argc){swan_irgen_fail(g,&e->token,"function call argument count mismatch");return 0;}
        SwanIRInst in={0};in.op=SWAN_IR_CALL;in.type=callee->return_type;in.symbol=e->left->token.text;
        if(e->argc){in.incoming=calloc(e->argc,sizeof(*in.incoming));if(!in.incoming){swan_irgen_fail(g,&e->token,"out of memory lowering call arguments");return 0;}}
        in.incoming_count=e->argc;
        for(size_t i=0;i<e->argc;i++){
            uint32_t v=swan_irgen_expr_as(g,e->args[i],callee->parameter_types[i]);if(!v){free(in.incoming);return 0;}
            if(swan_ir_value_type_of(g->function,v)!=callee->parameter_types[i]){free(in.incoming);swan_irgen_fail(g,&e->token,"function call argument type mismatch");return 0;}
            in.incoming[i].value=v;
        }
        uint32_t result=swan_irgen_emit(g,in);
        if(in.type==SWAN_IR_VOID)free(in.incoming);
        return result;
    }
    if(e->kind==E_BINARY) {
        if(e->token.kind==T_AND||e->token.kind==T_OR){
            uint32_t lhs=swan_irgen_expr(g,e->left);if(!lhs)return 0;
            uint32_t condition=swan_irgen_compare_zero(g,lhs);if(!condition)return 0;
            size_t origin=g->block,rhs_block=g->function->block_count;
            if(!swan_ir_add_block(g->function,"logic.rhs"))return swan_irgen_fail(g,&e->token,"unable to create short-circuit RHS block");
            size_t merge_block=g->function->block_count;
            if(!swan_ir_add_block(g->function,"logic.end"))return swan_irgen_fail(g,&e->token,"unable to create short-circuit merge block");
            bool is_and=e->token.kind==T_AND;
            uint32_t short_value=swan_irgen_const(g,SWAN_IR_I1,is_and?0:1);
            if(!short_value)return 0;
            if(!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_COND_BR,condition,(uint32_t)(is_and?rhs_block:merge_block),(uint32_t)(is_and?merge_block:rhs_block)}))return 0;
            g->block=rhs_block;
            uint32_t rhs=swan_irgen_expr(g,e->right);if(!rhs)return 0;
            rhs=swan_irgen_compare_zero(g,rhs);if(!rhs)return 0;
            size_t rhs_end=g->block;
            if(!swan_irgen_open(g)||!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_BR,0,(uint32_t)merge_block,0}))return 0;
            g->block=merge_block;
            SwanIRInst phi={0};phi.op=SWAN_IR_PHI;phi.type=SWAN_IR_I1;phi.incoming=calloc(2,sizeof(*phi.incoming));
            if(!phi.incoming)return swan_irgen_fail(g,&e->token,"out of memory creating short-circuit PHI");
            phi.incoming_count=2;
            phi.incoming[0]=(SwanIRIncoming){short_value,(uint32_t)origin};
            phi.incoming[1]=(SwanIRIncoming){rhs,(uint32_t)rhs_end};
            uint32_t result=swan_irgen_emit(g,phi);
            if(!result)free(phi.incoming);
            return result;
        }
        if(e->token.kind==T_ASSIGN) {
            if(!e->left||e->left->kind!=E_NAME){swan_irgen_fail(g,&e->token,"assignment target must be a local variable");return 0;}
            SwanIRLocal *local=swan_irgen_local(g,e->left->token.text);
            if(!local||!local->memory){swan_irgen_fail(g,&e->token,"assignment target is not mutable");return 0;}
            uint32_t v=swan_irgen_expr(g,e->right);if(!v)return 0;
            if(swan_ir_value_type_of(g->function,v)!=local->type){swan_irgen_fail(g,&e->token,"assignment type mismatch");return 0;}
            SwanIRInst in={0};in.op=SWAN_IR_STORE;in.type=SWAN_IR_VOID;in.a=local->value;in.b=v;
            swan_irgen_emit(g,in);return v;
        }
        SwanIRType expected=e->left&&e->left->kind==E_INT?swan_irgen_infer_expr_type(g,e->right):swan_irgen_infer_expr_type(g,e->left);
        uint32_t a=swan_irgen_expr_as(g,e->left,expected);if(!a)return 0;
        uint32_t b=swan_irgen_expr_as(g,e->right,swan_ir_value_type_of(g->function,a));if(!b)return 0;
        SwanIRType t=swan_ir_value_type_of(g->function,a);
        if(t!=swan_ir_value_type_of(g->function,b)){swan_irgen_fail(g,&e->token,"binary operand type mismatch");return 0;}
        SwanIROp op;
        switch(e->token.kind){
            case T_PLUS:op=SWAN_IR_ADD;break;case T_MINUS:op=SWAN_IR_SUB;break;case T_STAR:op=SWAN_IR_MUL;break;
            case T_SLASH:case T_FLOORDIV:op=SWAN_IR_SDIV;break;case T_PERCENT:op=SWAN_IR_SREM;break;
            case T_EQ:op=SWAN_IR_ICMP_EQ;break;case T_NE:op=SWAN_IR_ICMP_NE;break;case T_LT:op=SWAN_IR_ICMP_SLT;break;
            case T_LE:op=SWAN_IR_ICMP_SLE;break;case T_GT:op=SWAN_IR_ICMP_SGT;break;case T_GE:op=SWAN_IR_ICMP_SGE;break;
            case T_AND:op=SWAN_IR_AND;break;case T_OR:op=SWAN_IR_OR;break;
            default:swan_irgen_fail(g,&e->token,"unsupported binary operator");return 0;
        }
        SwanIRInst in={0};in.op=op;in.type=(op>=SWAN_IR_ICMP_EQ&&op<=SWAN_IR_ICMP_SGE)?SWAN_IR_I1:t;in.a=a;in.b=b;
        return swan_irgen_emit(g,in);
    }
    swan_irgen_fail(g,&e->token,"expression is not supported by the portable IR backend");return 0;
}
static inline bool swan_irgen_statements(SwanIRGen *g,Stmt **body,size_t count);
static inline bool swan_irgen_if(SwanIRGen *g,Stmt *s) {
    uint32_t cond=swan_irgen_expr(g,s->condition);if(!cond)return false;cond=swan_irgen_compare_zero(g,cond);if(!cond)return false;
    size_t then_id=g->function->block_count;
    if(!swan_ir_add_block(g->function,"if.then"))return swan_irgen_fail(g,&s->token,"out of memory creating if block");
    size_t else_id=g->function->block_count;
    if(!swan_ir_add_block(g->function,"if.else"))return swan_irgen_fail(g,&s->token,"out of memory creating else block");
    size_t merge_id=g->function->block_count;
    if(!swan_ir_add_block(g->function,"if.end"))return swan_irgen_fail(g,&s->token,"out of memory creating merge block");
    size_t local_mark=g->local_count;
    if(!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_COND_BR,cond,(uint32_t)then_id,(uint32_t)else_id}))return false;
    g->block=then_id;if(!swan_irgen_statements(g,s->body,s->body_count))return false;
    bool then_live=swan_irgen_open(g);g->local_count=local_mark;
    if(then_live&&!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_BR,0,(uint32_t)merge_id,0}))return false;
    g->block=else_id;if(!swan_irgen_statements(g,s->alternate,s->alt_count))return false;
    bool else_live=swan_irgen_open(g);g->local_count=local_mark;
    if(else_live&&!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_BR,0,(uint32_t)merge_id,0}))return false;
    g->block=merge_id;
    if(!then_live&&!else_live){swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_UNREACHABLE,0,0,0});}
    return true;
}
static inline bool swan_irgen_while(SwanIRGen *g,Stmt *s) {
    size_t cond_id=g->function->block_count;if(!swan_ir_add_block(g->function,"while.cond"))return false;
    size_t body_id=g->function->block_count;if(!swan_ir_add_block(g->function,"while.body"))return false;
    size_t end_id=g->function->block_count;if(!swan_ir_add_block(g->function,"while.end"))return false;
    if(!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_BR,0,(uint32_t)cond_id,0}))return false;
    g->block=cond_id;uint32_t cond=swan_irgen_expr(g,s->condition);if(!cond)return false;
    cond=swan_irgen_compare_zero(g,cond);if(!cond)return false;
    if(!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_COND_BR,cond,(uint32_t)body_id,(uint32_t)end_id}))return false;
    if(g->loop_depth>=64)return swan_irgen_fail(g,&s->token,"loop nesting limit exceeded");
    g->break_targets[g->loop_depth]=end_id;g->continue_targets[g->loop_depth]=cond_id;g->loop_depth++;
    size_t local_mark=g->local_count;
    g->block=body_id;bool ok=swan_irgen_statements(g,s->body,s->body_count);g->loop_depth--;g->local_count=local_mark;if(!ok)return false;
    if(swan_irgen_open(g)&&!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_BR,0,(uint32_t)cond_id,0}))return false;
    g->block=end_id;return true;
}
static inline bool swan_irgen_statements(SwanIRGen *g,Stmt **body,size_t count) {
    for(size_t i=0;i<count&&swan_irgen_open(g);i++) {
        Stmt *s=body[i];
        switch(s->kind) {
            case S_VARIABLE:{
                if(g->local_count>=256)return swan_irgen_fail(g,&s->token,"local variable limit exceeded");
                SwanIRType type=s->type?swan_irgen_type(s->type):SWAN_IR_VOID;
                uint32_t value=0;bool memory=s->mutable_value;
                if(memory){SwanIRInst alloc={0};alloc.op=SWAN_IR_ALLOCA;alloc.type=SWAN_IR_PTR;value=swan_irgen_emit(g,alloc);}
                if(s->value){uint32_t init=swan_irgen_expr(g,s->value);if(!init)return false;
                    if(type==SWAN_IR_VOID)type=swan_ir_value_type_of(g->function,init);
                    if(type!=swan_ir_value_type_of(g->function,init))return swan_irgen_fail(g,&s->token,"local initializer type mismatch");
                    if(memory){SwanIRInst store={0};store.op=SWAN_IR_STORE;store.type=SWAN_IR_VOID;store.a=value;store.b=init;swan_irgen_emit(g,store);}else value=init;
                }else if(!memory)return swan_irgen_fail(g,&s->token,"immutable local requires an initializer");
                if(type==SWAN_IR_VOID)return swan_irgen_fail(g,&s->token,"unsupported local variable type");
                g->locals[g->local_count++]=(SwanIRLocal){s->name,value,type,memory};break;
            }
            case S_RETURN:{
                uint32_t value=s->value?swan_irgen_expr(g,s->value):0;if(s->value&&!value)return false;
                if(s->value&&swan_ir_value_type_of(g->function,value)!=g->function->return_type)return swan_irgen_fail(g,&s->token,"return type mismatch");
                if(!s->value&&g->function->return_type!=SWAN_IR_VOID)return swan_irgen_fail(g,&s->token,"non-void function must return a value");
                if(!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_RET,value,0,0}))return false;break;
            }
            case S_IF:if(!swan_irgen_if(g,s))return false;break;
            case S_WHILE:if(!swan_irgen_while(g,s))return false;break;
            case S_BREAK:case S_CONTINUE:{
                if(!g->loop_depth)return swan_irgen_fail(g,&s->token,"loop control outside loop");
                size_t target=s->kind==S_BREAK?g->break_targets[g->loop_depth-1]:g->continue_targets[g->loop_depth-1];
                if(!swan_ir_terminate(swan_irgen_block(g),(SwanIRTerm){SWAN_IR_BR,0,(uint32_t)target,0}))return false;break;
            }
            case S_EXPR:if(s->value&&!swan_irgen_expr(g,s->value))return false;break;
            case S_PASS:break;
            case S_LOG:return swan_irgen_fail(g,&s->token,"log output is not yet supported by the portable SVM backend");
            default:return swan_irgen_fail(g,&s->token,"statement is not supported by the portable IR backend");
        }
    }
    return true;
}
static inline bool swan_irgen_program(Program *program,SwanIRModule *module,SwanError *error) {
    memset(module,0,sizeof(*module));
    for(size_t i=0;i<program->count;i++){
        Stmt *source=program->functions[i];SwanIRType params[16];
        if(source->param_count>16){swan_error(error,&source->token,"IR backend supports at most 16 parameters per function");goto fail;}
        for(size_t p=0;p<source->param_count;p++){params[p]=swan_irgen_type(source->param_types?source->param_types[p]:NULL);if(params[p]==SWAN_IR_VOID){swan_error(error,&source->token,"unsupported or missing parameter type");goto fail;}}
        SwanIRType ret=swan_irgen_type(source->type);
        SwanIRFunction *fn=swan_ir_add_function(module,source->name,ret,params,source->param_count);
        if(!fn){swan_error(error,&source->token,"out of memory creating IR function");goto fail;}
    }
    for(size_t i=0;i<program->count;i++){
        Stmt *source=program->functions[i];SwanIRFunction *fn=&module->functions[i];
        SwanIRBlock *entry=swan_ir_add_block(fn,"entry");if(!entry){swan_error(error,&source->token,"out of memory creating entry block");goto fail;}
        SwanIRGen g={0};g.module=module;g.function=fn;g.error=error;g.block=0;
        for(size_t p=0;p<source->param_count;p++){
            if(g.local_count>=256)goto fail;
            g.locals[g.local_count++]=(SwanIRLocal){source->params[p],(uint32_t)p+1,fn->parameter_types[p],false};
        }
        if(!swan_irgen_statements(&g,source->body,source->body_count))goto fail;
        if(swan_irgen_open(&g)){
            if(fn->return_type==SWAN_IR_VOID)swan_ir_terminate(swan_irgen_block(&g),(SwanIRTerm){SWAN_IR_RET,0,0,0});
            else {swan_error(error,&source->token,"non-void function can reach its end without returning");goto fail;}
        }
    }
    {char verify[256];if(!swan_ir_verify(module,verify,sizeof(verify))){swan_error(error,NULL,"generated IR failed verification: %s",verify);goto fail;}}
    return true;
fail:
    swan_ir_destroy(module);return false;
}
#endif
