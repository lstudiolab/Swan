#ifndef SWAN_IRGEN_H
#define SWAN_IRGEN_H
#include "swan/ir.h"
#include "swan/ast.h"

typedef struct {
    const char *names[64];
    uint32_t values[64];
    size_t count;
    SwanIRFunction *function;
    SwanIRBlock *block;
    SwanError *error;
} SwanIRGen;

static inline SwanIRType swan_irgen_type(const char *name) {
    if (!name || !strcmp(name, "int") || !strcmp(name, "i64")) return SWAN_IR_I64;
    if (!strcmp(name, "bool") || !strcmp(name, "i1")) return SWAN_IR_I1;
    if (!strcmp(name, "i32")) return SWAN_IR_I32;
    return SWAN_IR_VOID;
}
static inline uint32_t swan_irgen_const(SwanIRGen *g, SwanIRType type, int64_t value) {
    SwanIRInst in = {0}; in.op = SWAN_IR_CONST; in.type = type; in.immediate = value;
    return swan_ir_add_inst(g->function, g->block, in);
}
static inline uint32_t swan_irgen_expr(SwanIRGen *g, Expr *e) {
    if (!e) return 0;
    if (e->kind == E_INT || e->kind == E_BOOL) {
        char *end = NULL;
        int64_t value = e->kind == E_BOOL ? (e->token.kind == T_TRUE) : strtoll(e->token.text, &end, 10);
        if (e->kind == E_INT && (!end || *end)) {
            swan_error(g->error, &e->token, "integer literal is outside the supported IR range"); return 0;
        }
        return swan_irgen_const(g, e->kind == E_BOOL ? SWAN_IR_I1 : SWAN_IR_I64, value);
    }
    if (e->kind == E_NAME) {
        for (size_t i = g->count; i > 0; --i)
            if (!strcmp(g->names[i-1], e->token.text)) return g->values[i-1];
        swan_error(g->error, &e->token, "IR generation cannot resolve name '%s'", e->token.text); return 0;
    }
    if (e->kind == E_UNARY) {
        uint32_t operand = swan_irgen_expr(g, e->left); if (!operand) return 0;
        SwanIRType type = swan_ir_value_type_of(g->function, operand);
        if (e->token.kind == T_MINUS) {
            uint32_t zero = swan_irgen_const(g, type, 0); SwanIRInst in = {0};
            in.op = SWAN_IR_SUB; in.type = type; in.a = zero; in.b = operand;
            return swan_ir_add_inst(g->function, g->block, in);
        }
        if (e->token.kind == T_NOT) {
            uint32_t zero = swan_irgen_const(g, type, 0); SwanIRInst in = {0};
            in.op = SWAN_IR_ICMP_EQ; in.type = SWAN_IR_I1; in.a = operand; in.b = zero;
            return swan_ir_add_inst(g->function, g->block, in);
        }
    }
    if (e->kind == E_BINARY && e->token.kind != T_ASSIGN) {
        uint32_t a = swan_irgen_expr(g, e->left), b = swan_irgen_expr(g, e->right);
        if (!a || !b) return 0;
        SwanIRType type = swan_ir_value_type_of(g->function, a);
        SwanIROp op;
        switch (e->token.kind) {
            case T_PLUS: op=SWAN_IR_ADD; break; case T_MINUS: op=SWAN_IR_SUB; break;
            case T_STAR: op=SWAN_IR_MUL; break; case T_SLASH: case T_FLOORDIV: op=SWAN_IR_SDIV; break;
            case T_PERCENT: op=SWAN_IR_SREM; break; case T_EQ: op=SWAN_IR_ICMP_EQ; break;
            case T_NE: op=SWAN_IR_ICMP_NE; break; case T_LT: op=SWAN_IR_ICMP_SLT; break;
            case T_LE: op=SWAN_IR_ICMP_SLE; break; case T_GT: op=SWAN_IR_ICMP_SGT; break;
            case T_GE: op=SWAN_IR_ICMP_SGE; break;
            default: swan_error(g->error, &e->token, "operator is not yet supported by IR generation"); return 0;
        }
        SwanIRInst in = {0}; in.op=op; in.type=(op>=SWAN_IR_ICMP_EQ&&op<=SWAN_IR_ICMP_SGE)?SWAN_IR_I1:type; in.a=a; in.b=b;
        return swan_ir_add_inst(g->function, g->block, in);
    }
    swan_error(g->error, &e->token, "expression is not yet supported by IR generation");
    return 0;
}

/* Conservative first lowering profile: scalar parameters, immutable scalar
 * locals, and a single return path. Unsupported control-flow/memory constructs
 * fail explicitly instead of silently producing incorrect code. */
static inline bool swan_irgen_function(SwanIRModule *module, Stmt *source,
                                       SwanError *error) {
    SwanIRType params[16];
    for (size_t i=0; i<source->param_count; ++i) params[i]=SWAN_IR_I64;
    SwanIRFunction *fn=swan_ir_add_function(module, source->name, SWAN_IR_I64, params, source->param_count);
    if (!fn) { swan_error(error, &source->token, "out of memory creating IR function"); return false; }
    SwanIRBlock *entry=swan_ir_add_block(fn, "entry");
    if (!entry) { swan_error(error, &source->token, "out of memory creating IR block"); return false; }
    SwanIRGen g={0}; g.function=fn; g.block=entry; g.error=error;
    for (size_t i=0; i<source->param_count; ++i) {
        g.names[g.count]=source->params[i]; g.values[g.count]=(uint32_t)i+1; ++g.count;
    }
    bool returned=false;
    for (size_t i=0; i<source->body_count; ++i) {
        Stmt *s=source->body[i];
        if (s->kind==S_VARIABLE && !s->mutable_value && s->value && g.count<64) {
            uint32_t value=swan_irgen_expr(&g,s->value); if(!value)return false;
            g.names[g.count]=s->name; g.values[g.count]=value; ++g.count;
        } else if (s->kind==S_RETURN) {
            uint32_t value=s->value?swan_irgen_expr(&g,s->value):0;
            if (s->value && !value) return false;
            if (!swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_RET,value,0,0})) return false;
            returned=true; break;
        } else {
            swan_error(error,&s->token,"SVM IR profile currently supports immutable scalar locals and a single return; this statement requires the native backend");
            return false;
        }
    }
    if (!returned) {
        swan_error(error,&source->token,"SVM IR profile requires an explicit return statement");
        return false;
    }
    return true;
}
static inline bool swan_irgen_program(Program *program, SwanIRModule *module, SwanError *error) {
    memset(module,0,sizeof(*module));
    for (size_t i=0; i<program->count; ++i)
        if (!swan_irgen_function(module,program->functions[i],error)) { swan_ir_destroy(module); return false; }
    char verify[256];
    if (!swan_ir_verify(module,verify,sizeof(verify))) {
        swan_error(error,NULL,"generated IR failed verification: %s",verify);
        swan_ir_destroy(module); return false;
    }
    return true;
}
#endif
