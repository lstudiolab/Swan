#ifndef BREW_IRGEN_H
#define BREW_IRGEN_H
#include "brew/ir.h"
#include "brew/ast.h"
#include <errno.h>
#include <limits.h>

typedef struct {
    const char *name;
    uint32_t value;
    BrewIRType type;
    bool memory;
} BrewIRLocal;

typedef struct {
    BrewIRModule *module;
    BrewIRFunction *function;
    size_t block;
    BrewIRLocal *locals;
    size_t local_count,local_capacity;
    size_t *break_targets,*continue_targets;
    size_t loop_depth,loop_capacity,continue_capacity;
    BrewError *error;
} BrewIRGen;

static inline BrewIRType brew_irgen_type(const char *name) {
    if (!name || !strcmp(name,"int") || !strcmp(name,"i64")) return BREW_IR_I64;
    if (!strcmp(name,"bool") || !strcmp(name,"i1")) return BREW_IR_I1;
    if (!strcmp(name,"i8")) return BREW_IR_I8;
    if (!strcmp(name,"i16")) return BREW_IR_I16;
    if (!strcmp(name,"i32")) return BREW_IR_I32;
    if (!strcmp(name,"f32")) return BREW_IR_F32;
    if (!strcmp(name,"f64") || !strcmp(name,"float")) return BREW_IR_F64;
    if (!strcmp(name,"void") || !strcmp(name,"none")) return BREW_IR_VOID;
    return BREW_IR_VOID;
}
static inline BrewIRBlock *brew_irgen_block(BrewIRGen *g) {
    return g->block < g->function->block_count ? &g->function->blocks[g->block] : NULL;
}
static inline bool brew_irgen_open(BrewIRGen *g) {
    BrewIRBlock *b=brew_irgen_block(g); return b && !b->terminated;
}
static inline bool brew_irgen_fail(BrewIRGen *g,const Token *token,const char *format,...) {
    char detail[512];va_list args;va_start(args,format);
    vsnprintf(detail,sizeof(detail),format,args);va_end(args);
    brew_error(g->error,token,"%s",detail);return false;
}
static inline uint32_t brew_irgen_emit(BrewIRGen *g,BrewIRInst in) {
    BrewIRBlock *block=brew_irgen_block(g);
    if(!block){
        brew_irgen_fail(g,NULL,"cannot emit an instruction without an active IR block");
        return 0;
    }
    size_t previous_count=block->instruction_count;
    uint32_t id=brew_ir_add_inst(g->function,block,in);
    if((in.type!=BREW_IR_VOID&&!id)||
       (in.type==BREW_IR_VOID&&block->instruction_count!=previous_count+1)){
        brew_irgen_fail(g,NULL,"unable to append typed IR instruction");
        return 0;
    }
    return id;
}
static inline bool brew_irgen_emit_void(BrewIRGen *g,BrewIRInst instruction) {
    if(instruction.type!=BREW_IR_VOID){
        brew_irgen_fail(g,NULL,"void IR emission received a value-producing instruction");
        return false;
    }
    BrewIRBlock *block=brew_irgen_block(g);
    if(!block){
        brew_irgen_fail(g,NULL,"cannot emit an instruction without an active IR block");
        return false;
    }
    size_t previous_count=block->instruction_count;
    (void)brew_ir_add_inst(g->function,block,instruction);
    if(block->instruction_count!=previous_count+1){
        brew_irgen_fail(g,NULL,"unable to append void IR instruction");
        return false;
    }
    return true;
}
static inline uint32_t brew_irgen_const(BrewIRGen *g,BrewIRType type,int64_t value) {
    BrewIRInst in={0};in.op=BREW_IR_CONST;in.type=type;in.immediate=value;return brew_irgen_emit(g,in);
}
static inline uint32_t brew_irgen_compare_zero(BrewIRGen *g,uint32_t value) {
    BrewIRType t=brew_ir_value_type_of(g->function,value);
    if(t==BREW_IR_I1)return value;
    uint32_t zero=brew_irgen_const(g,t,0);BrewIRInst in={0};
    in.op=BREW_IR_ICMP_NE;in.type=BREW_IR_I1;in.a=value;in.b=zero;return brew_irgen_emit(g,in);
}
static inline bool brew_irgen_add_local(BrewIRGen *g,BrewIRLocal local){
    if(g->local_count==SIZE_MAX||!brew_reserve_array((void**)&g->locals,&g->local_capacity,g->local_count+1,sizeof(*g->locals)))
        return brew_irgen_fail(g,NULL,"out of memory growing IR local table");
    g->locals[g->local_count++]=local;return true;
}
static inline bool brew_irgen_push_loop(BrewIRGen *g,size_t break_target,size_t continue_target){
    if(g->loop_depth==SIZE_MAX)return brew_irgen_fail(g,NULL,"loop nesting depth overflow");
    size_t needed=g->loop_depth+1;
    if(!brew_reserve_array((void**)&g->break_targets,&g->loop_capacity,needed,sizeof(*g->break_targets)))
        return brew_irgen_fail(g,NULL,"out of memory growing loop target stack");
    if(!brew_reserve_array((void**)&g->continue_targets,&g->continue_capacity,needed,sizeof(*g->continue_targets)))
        return brew_irgen_fail(g,NULL,"out of memory growing loop target stack");
    g->break_targets[g->loop_depth]=break_target;g->continue_targets[g->loop_depth]=continue_target;g->loop_depth++;return true;
}
static inline BrewIRLocal *brew_irgen_local(BrewIRGen *g,const char *name) {
    for(size_t i=g->local_count;i>0;i--)if(!strcmp(g->locals[i-1].name,name))return &g->locals[i-1];
    return NULL;
}
static inline uint32_t brew_irgen_expr(BrewIRGen *g,Expr *e);
static inline bool brew_irgen_integer_literal(Expr *e,int64_t *value){
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

static inline BrewIRType brew_irgen_infer_expr_type(BrewIRGen *g,Expr *e){
    if(!e)return BREW_IR_VOID;
    if(e->kind==E_BOOL)return BREW_IR_I1;
    if(e->kind==E_INT)return BREW_IR_I64;
    if(e->kind==E_NAME){BrewIRLocal *local=brew_irgen_local(g,e->token.text);return local?local->type:BREW_IR_VOID;}
    if(e->kind==E_CALL&&e->left&&e->left->kind==E_NAME){
        for(size_t i=0;i<g->module->function_count;i++)if(!strcmp(g->module->functions[i].name,e->left->token.text))return g->module->functions[i].return_type;
    }
    if(e->kind==E_UNARY)return e->token.kind==T_NOT?BREW_IR_I1:brew_irgen_infer_expr_type(g,e->left);
    if(e->kind==E_BINARY){
        if(e->token.kind==T_EQ||e->token.kind==T_NE||e->token.kind==T_LT||e->token.kind==T_LE||e->token.kind==T_GT||e->token.kind==T_GE||e->token.kind==T_AND||e->token.kind==T_OR)return BREW_IR_I1;
        BrewIRType left=brew_irgen_infer_expr_type(g,e->left);int64_t literal_value;
        return brew_irgen_integer_literal(e->left,&literal_value)?brew_irgen_infer_expr_type(g,e->right):left;
    }
    return BREW_IR_VOID;
}
static inline uint32_t brew_irgen_expr_as(BrewIRGen *g,Expr *e,BrewIRType expected){
    int64_t literal;
    if(brew_irgen_integer_literal(e,&literal)){
        unsigned bits=expected==BREW_IR_I1?1:expected==BREW_IR_I8?8:expected==BREW_IR_I16?16:expected==BREW_IR_I32?32:expected==BREW_IR_I64?64:0;
        if(!bits){brew_irgen_fail(g,&e->token,"integer literal cannot initialize this type");return 0;}
        int64_t min=bits==64?INT64_MIN:-(INT64_C(1)<<(bits-1));
        int64_t max=bits==64?INT64_MAX:(INT64_C(1)<<(bits-1))-1;
        if(literal<min||literal>max){brew_irgen_fail(g,&e->token,"integer literal is outside the destination type range");return 0;}
        return brew_irgen_const(g,expected,literal);
    }
    uint32_t value=brew_irgen_expr(g,e);if(!value)return 0;
    if(brew_ir_value_type_of(g->function,value)!=expected){brew_irgen_fail(g,&e->token,"expression type does not match its context");return 0;}
    return value;
}
static inline uint32_t brew_irgen_expr(BrewIRGen *g,Expr *e) {
    if(!e)return 0;
    if(e->kind==E_INT||e->kind==E_BOOL) {
        char *end=NULL;errno=0;int64_t v=e->kind==E_BOOL?(e->token.kind==T_TRUE):strtoll(e->token.text,&end,10);
        if(e->kind==E_INT&&(errno==ERANGE||!end||end==e->token.text||*end)){brew_irgen_fail(g,&e->token,"integer literal is outside the supported IR range");return 0;}
        return brew_irgen_const(g,e->kind==E_BOOL?BREW_IR_I1:BREW_IR_I64,v);
    }
    if(e->kind==E_NAME) {
        BrewIRLocal *local=brew_irgen_local(g,e->token.text);
        if(!local){brew_irgen_fail(g,&e->token,"IR generation cannot resolve local name");return 0;}
        if(!local->memory)return local->value;
        BrewIRInst in={0};in.op=BREW_IR_LOAD;in.type=local->type;in.a=local->value;return brew_irgen_emit(g,in);
    }
    if(e->kind==E_UNARY) {
        /*
         * Parse a signed integer literal as one value before lowering its
         * positive magnitude. In particular, INT64_MIN has no representable
         * positive int64_t counterpart, so recursively lowering the operand
         * through strtoll() incorrectly rejects it.
         */
        int64_t signed_literal;
        if(e->token.kind==T_MINUS&&brew_irgen_integer_literal(e,&signed_literal))
            return brew_irgen_const(g,brew_irgen_infer_expr_type(g,e),signed_literal);
        uint32_t a=brew_irgen_expr(g,e->left);if(!a)return 0;
        BrewIRType t=brew_ir_value_type_of(g->function,a);
        if(e->token.kind==T_MINUS){uint32_t z=brew_irgen_const(g,t,0);BrewIRInst in={0};in.op=BREW_IR_SUB;in.type=t;in.a=z;in.b=a;return brew_irgen_emit(g,in);}
        if(e->token.kind==T_NOT){uint32_t z=brew_irgen_const(g,t,0);BrewIRInst in={0};in.op=BREW_IR_ICMP_EQ;in.type=BREW_IR_I1;in.a=a;in.b=z;return brew_irgen_emit(g,in);}
        brew_irgen_fail(g,&e->token,"unsupported unary operator");return 0;
    }
    if(e->kind==E_CALL) {
        if(!e->left||e->left->kind!=E_NAME){brew_irgen_fail(g,&e->token,"only direct function calls are supported");return 0;}
        size_t index=g->module->function_count;
        for(size_t i=0;i<g->module->function_count;i++)if(!strcmp(g->module->functions[i].name,e->left->token.text)){index=i;break;}
        if(index==g->module->function_count){ brew_irgen_fail(g,&e->token,"unknown function '%s' in IR call",e->left->token.text);return 0;}
        BrewIRFunction *callee=&g->module->functions[index];
        if(callee->parameter_count!=e->argc){brew_irgen_fail(g,&e->token,"function call argument count mismatch");return 0;}
        BrewIRInst in={0};in.op=BREW_IR_CALL;in.type=callee->return_type;in.symbol=e->left->token.text;
        if(e->argc){in.incoming=calloc(e->argc,sizeof(*in.incoming));if(!in.incoming){brew_irgen_fail(g,&e->token,"out of memory lowering call arguments");return 0;}}
        in.incoming_count=e->argc;
        for(size_t i=0;i<e->argc;i++){
            uint32_t v=brew_irgen_expr_as(g,e->args[i],callee->parameter_types[i]);if(!v){free(in.incoming);return 0;}
            if(brew_ir_value_type_of(g->function,v)!=callee->parameter_types[i]){free(in.incoming);brew_irgen_fail(g,&e->token,"function call argument type mismatch");return 0;}
            in.incoming[i].value=v;
        }
        if(in.type==BREW_IR_VOID){
            if(!brew_irgen_emit_void(g,in)){
                free(in.incoming);
            }
            return 0;
        }
        uint32_t result=brew_irgen_emit(g,in);
        if(!result)free(in.incoming);
        return result;
    }
    if(e->kind==E_BINARY) {
        if(e->token.kind==T_AND||e->token.kind==T_OR){
            uint32_t lhs=brew_irgen_expr(g,e->left);if(!lhs)return 0;
            uint32_t condition=brew_irgen_compare_zero(g,lhs);if(!condition)return 0;
            size_t origin=g->block,rhs_block=g->function->block_count;
            if(!brew_ir_add_block(g->function,"logic.rhs"))return brew_irgen_fail(g,&e->token,"unable to create short-circuit RHS block");
            size_t merge_block=g->function->block_count;
            if(!brew_ir_add_block(g->function,"logic.end"))return brew_irgen_fail(g,&e->token,"unable to create short-circuit merge block");
            bool is_and=e->token.kind==T_AND;
            uint32_t short_value=brew_irgen_const(g,BREW_IR_I1,is_and?0:1);
            if(!short_value)return 0;
            if(!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_COND_BR,condition,(uint32_t)(is_and?rhs_block:merge_block),(uint32_t)(is_and?merge_block:rhs_block)}))return 0;
            g->block=rhs_block;
            uint32_t rhs=brew_irgen_expr(g,e->right);if(!rhs)return 0;
            rhs=brew_irgen_compare_zero(g,rhs);if(!rhs)return 0;
            size_t rhs_end=g->block;
            if(!brew_irgen_open(g)||!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_BR,0,(uint32_t)merge_block,0}))return 0;
            g->block=merge_block;
            BrewIRInst phi={0};phi.op=BREW_IR_PHI;phi.type=BREW_IR_I1;phi.incoming=calloc(2,sizeof(*phi.incoming));
            if(!phi.incoming)return brew_irgen_fail(g,&e->token,"out of memory creating short-circuit PHI");
            phi.incoming_count=2;
            phi.incoming[0]=(BrewIRIncoming){short_value,(uint32_t)origin};
            phi.incoming[1]=(BrewIRIncoming){rhs,(uint32_t)rhs_end};
            uint32_t result=brew_irgen_emit(g,phi);
            if(!result)free(phi.incoming);
            return result;
        }
        if(e->token.kind==T_ASSIGN) {
            if(!e->left||e->left->kind!=E_NAME){brew_irgen_fail(g,&e->token,"assignment target must be a local variable");return 0;}
            BrewIRLocal *local=brew_irgen_local(g,e->left->token.text);
            if(!local||!local->memory){brew_irgen_fail(g,&e->token,"assignment target is not mutable");return 0;}
            uint32_t v=brew_irgen_expr_as(g,e->right,local->type);if(!v)return 0;
            BrewIRInst in={0};in.op=BREW_IR_STORE;in.type=BREW_IR_VOID;in.a=local->value;in.b=v;
            if(!brew_irgen_emit_void(g,in))return 0;
            return v;
        }
        int64_t literal_value;BrewIRType expected=brew_irgen_integer_literal(e->left,&literal_value)?brew_irgen_infer_expr_type(g,e->right):brew_irgen_infer_expr_type(g,e->left);
        uint32_t a=brew_irgen_expr_as(g,e->left,expected);if(!a)return 0;
        uint32_t b=brew_irgen_expr_as(g,e->right,brew_ir_value_type_of(g->function,a));if(!b)return 0;
        BrewIRType t=brew_ir_value_type_of(g->function,a);
        if(t!=brew_ir_value_type_of(g->function,b)){brew_irgen_fail(g,&e->token,"binary operand type mismatch");return 0;}
        BrewIROp op;
        switch(e->token.kind){
            case T_PLUS:op=BREW_IR_ADD;break;case T_MINUS:op=BREW_IR_SUB;break;case T_STAR:op=BREW_IR_MUL;break;
            case T_SLASH:case T_FLOORDIV:op=BREW_IR_SDIV;break;case T_PERCENT:op=BREW_IR_SREM;break;
            case T_EQ:op=BREW_IR_ICMP_EQ;break;case T_NE:op=BREW_IR_ICMP_NE;break;case T_LT:op=BREW_IR_ICMP_SLT;break;
            case T_LE:op=BREW_IR_ICMP_SLE;break;case T_GT:op=BREW_IR_ICMP_SGT;break;case T_GE:op=BREW_IR_ICMP_SGE;break;
            case T_AND:op=BREW_IR_AND;break;case T_OR:op=BREW_IR_OR;break;
            default:brew_irgen_fail(g,&e->token,"unsupported binary operator");return 0;
        }
        BrewIRInst in={0};in.op=op;in.type=(op>=BREW_IR_ICMP_EQ&&op<=BREW_IR_ICMP_SGE)?BREW_IR_I1:t;in.a=a;in.b=b;
        return brew_irgen_emit(g,in);
    }
    brew_irgen_fail(g,&e->token,"expression is not supported by the portable IR backend");return 0;
}
static inline bool brew_irgen_statements(BrewIRGen *g,Stmt **body,size_t count);
static inline bool brew_irgen_if(BrewIRGen *g,Stmt *s) {
    uint32_t cond=brew_irgen_expr(g,s->condition);if(!cond)return false;cond=brew_irgen_compare_zero(g,cond);if(!cond)return false;
    size_t then_id=g->function->block_count;
    if(!brew_ir_add_block(g->function,"if.then"))return brew_irgen_fail(g,&s->token,"out of memory creating if block");
    size_t else_id=g->function->block_count;
    if(!brew_ir_add_block(g->function,"if.else"))return brew_irgen_fail(g,&s->token,"out of memory creating else block");
    size_t merge_id=g->function->block_count;
    if(!brew_ir_add_block(g->function,"if.end"))return brew_irgen_fail(g,&s->token,"out of memory creating merge block");
    size_t local_mark=g->local_count;
    if(!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_COND_BR,cond,(uint32_t)then_id,(uint32_t)else_id}))return false;
    g->block=then_id;if(!brew_irgen_statements(g,s->body,s->body_count))return false;
    bool then_live=brew_irgen_open(g);g->local_count=local_mark;
    if(then_live&&!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_BR,0,(uint32_t)merge_id,0}))return false;
    g->block=else_id;if(!brew_irgen_statements(g,s->alternate,s->alt_count))return false;
    bool else_live=brew_irgen_open(g);g->local_count=local_mark;
    if(else_live&&!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_BR,0,(uint32_t)merge_id,0}))return false;
    g->block=merge_id;
    if(!then_live&&!else_live){brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_UNREACHABLE,0,0,0});}
    return true;
}
static inline bool brew_irgen_while(BrewIRGen *g,Stmt *s) {
    size_t cond_id=g->function->block_count;if(!brew_ir_add_block(g->function,"while.cond"))return false;
    size_t body_id=g->function->block_count;if(!brew_ir_add_block(g->function,"while.body"))return false;
    size_t end_id=g->function->block_count;if(!brew_ir_add_block(g->function,"while.end"))return false;
    if(!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_BR,0,(uint32_t)cond_id,0}))return false;
    g->block=cond_id;uint32_t cond=brew_irgen_expr(g,s->condition);if(!cond)return false;
    cond=brew_irgen_compare_zero(g,cond);if(!cond)return false;
    if(!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_COND_BR,cond,(uint32_t)body_id,(uint32_t)end_id}))return false;
    if(!brew_irgen_push_loop(g,end_id,cond_id))return false;
    size_t local_mark=g->local_count;
    g->block=body_id;bool ok=brew_irgen_statements(g,s->body,s->body_count);g->loop_depth--;g->local_count=local_mark;if(!ok)return false;
    if(brew_irgen_open(g)&&!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_BR,0,(uint32_t)cond_id,0}))return false;
    g->block=end_id;return true;
}
static inline bool brew_irgen_statements(BrewIRGen *g,Stmt **body,size_t count) {
    for(size_t i=0;i<count&&brew_irgen_open(g);i++) {
        Stmt *s=body[i];
        switch(s->kind) {
            case S_VARIABLE:{
                BrewIRType type=s->type?brew_irgen_type(s->type):BREW_IR_VOID;
                uint32_t value=0;bool memory=s->mutable_value;
                if(memory){
                    BrewIRInst alloc={0};
                    alloc.op=BREW_IR_ALLOCA;
                    alloc.type=BREW_IR_PTR;
                    value=brew_irgen_emit(g,alloc);
                    if(!value)return false;
                }
                if(s->value){uint32_t init=type==BREW_IR_VOID?brew_irgen_expr(g,s->value):brew_irgen_expr_as(g,s->value,type);if(!init)return false;
                    if(type==BREW_IR_VOID)type=brew_ir_value_type_of(g->function,init);
                    if(memory){BrewIRInst store={0};store.op=BREW_IR_STORE;store.type=BREW_IR_VOID;store.a=value;store.b=init;if(!brew_irgen_emit_void(g,store))return false;}else value=init;
                }else if(!memory)return brew_irgen_fail(g,&s->token,"immutable local requires an initializer");
                if(type==BREW_IR_VOID)return brew_irgen_fail(g,&s->token,"unsupported local variable type");
                if(!brew_irgen_add_local(g,(BrewIRLocal){s->name,value,type,memory}))return false;
                break;
            }
            case S_RETURN:{
                uint32_t value=s->value?brew_irgen_expr_as(g,s->value,g->function->return_type):0;if(s->value&&!value)return false;
                if(!s->value&&g->function->return_type!=BREW_IR_VOID)return brew_irgen_fail(g,&s->token,"non-void function must return a value");
                if(!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_RET,value,0,0}))return false;
                break;
            }
            case S_IF:if(!brew_irgen_if(g,s))return false;break;
            case S_WHILE:if(!brew_irgen_while(g,s))return false;break;
            case S_BREAK:case S_CONTINUE:{
                if(!g->loop_depth)return brew_irgen_fail(g,&s->token,"loop control outside loop");
                size_t target=s->kind==S_BREAK?g->break_targets[g->loop_depth-1]:g->continue_targets[g->loop_depth-1];
                if(!brew_ir_terminate(brew_irgen_block(g),(BrewIRTerm){BREW_IR_BR,0,(uint32_t)target,0}))return false;
                break;
            }
            case S_EXPR:
                if(s->value){
                    uint32_t result=brew_irgen_expr(g,s->value);
                    if(!result){
                        /*
                         * Void calls intentionally have no SSA result ID.
                         * Detect successful emission from the final instruction
                         * rather than dereferencing the optional diagnostic sink.
                         */
                        BrewIRBlock *block=brew_irgen_block(g);
                        bool void_call=s->value->kind==E_CALL&&block&&
                            block->instruction_count>0&&
                            block->instructions[block->instruction_count-1].op==BREW_IR_CALL&&
                            block->instructions[block->instruction_count-1].type==BREW_IR_VOID;
                        if(!void_call)return false;
                    }
                }
                break;
            case S_PASS:break;
            case S_LOG:return brew_irgen_fail(g,&s->token,"log output is not yet supported by the portable SVM backend");
            default:return brew_irgen_fail(g,&s->token,"statement is not supported by the portable IR backend");
        }
    }
    return true;
}
static inline bool brew_irgen_include_function(const Program *program,size_t index){
    if(program->codegen_source_index<0)
        return !program->reachability_computed||program->function_reachable[index];
    const Stmt *function=program->functions[index];
    if(function->source_index==(size_t)program->codegen_source_index)
        return !program->reachability_computed||program->function_reachable[index]||function->exported;
    return program->codegen_include_external_functions&&
           (!program->reachability_computed||program->function_reachable[index]);
}
static inline bool brew_irgen_program(Program *program,BrewIRModule *module,BrewError *error) {
    if(error)memset(error,0,sizeof(*error));
    if(!program||!module){
        brew_error(error,NULL,"IR generation requires a program and output module");
        return false;
    }
    if(program->count>SIZE_MAX/sizeof(BrewIRFunction *)){
        brew_error(error,NULL,"IR function table size overflow");
        return false;
    }
    for(size_t i=0;i<program->count;i++){
        if(!program->functions[i]||!program->functions[i]->name||
           !program->functions[i]->name[0]){
            brew_error(error,program->functions[i]?&program->functions[i]->token:NULL,
                       "IR generation received an invalid function declaration");
            return false;
        }
    }
    memset(module,0,sizeof(*module));
    module->allow_no_main=program->codegen_source_index>=0;
    for(size_t i=0;i<program->count;i++){
        if(!brew_irgen_include_function(program,i))continue;
        Stmt *source=program->functions[i];
        if(source->param_count>UINT32_MAX-1u||source->param_count>SIZE_MAX/sizeof(BrewIRType)){brew_error(error,&source->token,"function parameter count exceeds the IR value-ID range");goto fail;}
        BrewIRType *params=source->param_count?malloc(source->param_count*sizeof(*params)):NULL;
        if(source->param_count&&!params){brew_error(error,&source->token,"out of memory lowering function parameters");goto fail;}
        bool valid=true;for(size_t p=0;p<source->param_count;p++){params[p]=brew_irgen_type(source->param_types?source->param_types[p]:NULL);if(params[p]==BREW_IR_VOID){brew_error(error,&source->token,"unsupported or missing parameter type");valid=false;break;}}
        BrewIRType ret=brew_irgen_type(source->type);
        BrewIRFunction *created=valid?brew_ir_add_function(module,source->link_name?source->link_name:source->name,ret,params,source->param_count):NULL;free(params);
        if(!valid)goto fail;
        if(!created){brew_error(error,&source->token,"out of memory creating IR function");goto fail;}
        created->exported=source->exported;
        created->external=source->external||(program->codegen_source_index>=0&&
            source->source_index!=(size_t)program->codegen_source_index);
    }
    if(!module->function_count&&program->codegen_source_index<0){brew_error(error,NULL,"IR lowering has no reachable functions");goto fail;}
    BrewIRGen g={0};
    for(size_t i=0;i<program->count;i++){
        if(!brew_irgen_include_function(program,i))continue;
        Stmt *source=program->functions[i];
        if(source->external||(program->codegen_source_index>=0&&
           source->source_index!=(size_t)program->codegen_source_index))continue;
        BrewIRFunction *fn=NULL;
        for(size_t fi=0;fi<module->function_count;fi++)if(!strcmp(module->functions[fi].name,source->link_name?source->link_name:source->name)){fn=&module->functions[fi];break;}
        if(!fn){brew_error(error,&source->token,"internal IR function mapping failure");goto fail;}
        BrewIRBlock *entry=brew_ir_add_block(fn,"entry");if(!entry){brew_error(error,&source->token,"out of memory creating entry block");goto fail;}
        g.module=module;g.function=fn;g.error=error;g.block=0;g.local_count=0;g.loop_depth=0;
        for(size_t p=0;p<source->param_count;p++){
            if(!brew_irgen_add_local(&g,(BrewIRLocal){source->params[p],(uint32_t)p+1,fn->parameter_types[p],false}))goto fail;
        }
        if(!brew_irgen_statements(&g,source->body,source->body_count))goto fail;
        if(brew_irgen_open(&g)){
            if(fn->return_type==BREW_IR_VOID)brew_ir_terminate(brew_irgen_block(&g),(BrewIRTerm){BREW_IR_RET,0,0,0});
            else {brew_error(error,&source->token,"non-void function can reach its end without returning");goto fail;}
        }
    }
    free(g.locals);free(g.break_targets);free(g.continue_targets);memset(&g,0,sizeof(g));
    {char verify[256];if(!brew_ir_verify(module,verify,sizeof(verify))){brew_error(error,NULL,"generated IR failed verification: %s",verify);goto fail;}}
    return true;
fail:
    free(g.locals);free(g.break_targets);free(g.continue_targets);
    brew_ir_destroy(module);return false;
}
#endif
