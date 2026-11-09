#ifndef SWAN_SIL_BRIDGE_H
#define SWAN_SIL_BRIDGE_H

#include "swan/ir.h"
#include "swan/ast.h"
#include "swan/irgen.h"
#include <stdint.h>
#include <limits.h>

extern int64_t swan_fn_sil_emit_param(uint32_t *, int64_t, int64_t);
extern int64_t swan_fn_sil_emit_const(uint32_t *, int64_t, int64_t);
extern int64_t swan_fn_sil_emit_binary(uint32_t *, int64_t, int64_t);
extern int64_t swan_fn_sil_emit_return(uint32_t *, int64_t, int64_t);
extern int64_t swan_fn_sil_optimize(uint32_t *, int64_t, uint32_t *);

static inline int swan_sil_type_code(SwanIRType type) {
    if (type == SWAN_IR_I32) return 1;
    if (type == SWAN_IR_I64) return 2;
    if (type == SWAN_IR_I1) return 3;
    return 0;
}
static inline SwanIRType swan_sil_ir_type(int code) {
    if (code == 1) return SWAN_IR_I32;
    if (code == 2) return SWAN_IR_I64;
    if (code == 3) return SWAN_IR_I1;
    return SWAN_IR_VOID;
}
static inline SwanIRType swan_sil_decl_type(const char *name) {
    if (!name || !strcmp(name, "int") || !strcmp(name, "i64")) return SWAN_IR_I64;
    if (!strcmp(name, "i32")) return SWAN_IR_I32;
    if (!strcmp(name, "bool") || !strcmp(name, "i1")) return SWAN_IR_I1;
    return SWAN_IR_VOID;
}
static inline SwanIRType swan_sil_param_type(const Stmt *fn, size_t index) {
    return swan_sil_decl_type(fn->param_types ? fn->param_types[index] : NULL);
}
static inline SwanIRType swan_sil_hint(Expr *expr, const Stmt *fn) {
    if (!expr) return SWAN_IR_VOID;
    if (expr->kind == E_BOOL) return SWAN_IR_I1;
    if (expr->kind == E_INT) return SWAN_IR_VOID;
    if (expr->kind == E_NAME) {
        for (size_t i = 0; i < fn->param_count; ++i)
            if (!strcmp(fn->params[i], expr->token.text)) return swan_sil_param_type(fn, i);
        return SWAN_IR_VOID;
    }
    if (expr->kind == E_BINARY) {
        if (expr->token.kind == T_EQ || expr->token.kind == T_LT) return SWAN_IR_I1;
        SwanIRType left = swan_sil_hint(expr->left, fn);
        return left != SWAN_IR_VOID ? left : swan_sil_hint(expr->right, fn);
    }
    return SWAN_IR_VOID;
}
static inline bool swan_sil_emit_const_value(uint32_t *state, int64_t *count,
                                              uint32_t *next_value, int64_t value,
                                              SwanIRType type, uint32_t *result) {
    int code = swan_sil_type_code(type);
    if (!code || value < 0 || value > 65535 || *next_value >= 256) return false;
    int64_t packed = (int64_t)(*next_value) * 1048576 + (int64_t)code * 65536 + value;
    int64_t next = swan_fn_sil_emit_const(state, *count, packed);
    if (next != *count + 1) return false;
    *count = next;
    *result = (*next_value)++;
    return true;
}
static inline bool swan_sil_expr(Expr *expr, const Stmt *fn, uint32_t *state,
                                  int64_t *count, uint32_t *next_value,
                                  SwanIRType expected, uint32_t *result,
                                  SwanIRType *result_type) {
    if (!expr) return false;
    if (expr->kind == E_INT || expr->kind == E_BOOL) {
        int64_t value = 0;
        SwanIRType type = expr->kind == E_BOOL ? SWAN_IR_I1 :
                          expected != SWAN_IR_VOID ? expected : SWAN_IR_I64;
        if (expr->kind == E_BOOL) value = expr->token.kind == T_TRUE ? 1 : 0;
        else if (!swan_irgen_integer_literal(expr, &value) || value < 0 || value > 65535) return false;
        if (expected != SWAN_IR_VOID && type != expected) return false;
        if (!swan_sil_emit_const_value(state, count, next_value, value, type, result)) return false;
        *result_type = type;
        return true;
    }
    if (expr->kind == E_NAME) {
        for (size_t i = 0; i < fn->param_count; ++i) {
            if (strcmp(fn->params[i], expr->token.text)) continue;
            SwanIRType type = swan_sil_param_type(fn, i);
            if (type == SWAN_IR_VOID || (expected != SWAN_IR_VOID && type != expected)) return false;
            *result = (uint32_t)i + 1;
            *result_type = type;
            return true;
        }
        return false;
    }
    if (expr->kind != E_BINARY) return false;

    int opcode = 0;
    bool comparison = false;
    switch (expr->token.kind) {
        case T_PLUS: opcode = 2; break;
        case T_MINUS: opcode = 3; break;
        case T_STAR: opcode = 4; break;
        case T_EQ: opcode = 7; comparison = true; break;
        case T_LT: opcode = 8; comparison = true; break;
        default: return false;
    }
    SwanIRType operand_type = swan_sil_hint(expr->left, fn);
    if (operand_type == SWAN_IR_VOID) operand_type = swan_sil_hint(expr->right, fn);
    if (operand_type == SWAN_IR_VOID) operand_type = SWAN_IR_I64;
    if (operand_type != SWAN_IR_I32 && operand_type != SWAN_IR_I64 && operand_type != SWAN_IR_I1) return false;
    if (!comparison && expected != SWAN_IR_VOID && expected != operand_type) return false;
    if (comparison && expected != SWAN_IR_VOID && expected != SWAN_IR_I1) return false;
    if (comparison && opcode == 8 && operand_type == SWAN_IR_I1) return false;
    if (comparison && (expr->left->kind == E_BINARY || expr->right->kind == E_BINARY)) return false;

    uint32_t left = 0, right = 0;
    SwanIRType left_type = SWAN_IR_VOID, right_type = SWAN_IR_VOID;
    if (!swan_sil_expr(expr->left, fn, state, count, next_value, operand_type, &left, &left_type) ||
        !swan_sil_expr(expr->right, fn, state, count, next_value, operand_type, &right, &right_type) ||
        left_type != operand_type || right_type != operand_type || *next_value >= 256) return false;

    uint32_t destination = (*next_value)++;
    int encoded_type = swan_sil_type_code(comparison ? SWAN_IR_I1 : operand_type);
    int64_t packed = (int64_t)(encoded_type * 16 + opcode) * 16777216 +
                     (int64_t)destination * 65536 + (int64_t)left * 256 + right;
    int64_t next = swan_fn_sil_emit_binary(state, *count, packed);
    if (next != *count + 1) return false;
    *count = next;
    *result = destination;
    *result_type = comparison ? SWAN_IR_I1 : operand_type;
    return true;
}
static inline bool swan_sil_lower_function(const Stmt *fn, SwanIRModule *module) {
    if (!fn || fn->imported || fn->body_count != 1 || fn->body[0]->kind != S_RETURN ||
        !fn->body[0]->value || fn->param_count > 254 || fn->param_count > 16) return false;
    SwanIRType return_type = swan_sil_decl_type(fn->type);
    if (return_type == SWAN_IR_VOID) return false;

    SwanIRType parameter_types[16];
    uint32_t state[256 * 5] = {0};
    uint32_t workspace[1024] = {0};
    int64_t count = 0;
    uint32_t next_value = (uint32_t)fn->param_count + 1;

    for (size_t i = 0; i < fn->param_count; ++i) {
        SwanIRType type = swan_sil_param_type(fn, i);
        int code = swan_sil_type_code(type);
        if (!code) return false;
        parameter_types[i] = type;
        int64_t packed = (int64_t)(i + 1) * 1048576 + (int64_t)code * 65536 + (int64_t)i;
        int64_t next = swan_fn_sil_emit_param(state, count, packed);
        if (next != count + 1) return false;
        count = next;
    }

    uint32_t return_value = 0;
    SwanIRType expression_type = SWAN_IR_VOID;
    if (!swan_sil_expr(fn->body[0]->value, fn, state, &count, &next_value,
                       return_type, &return_value, &expression_type) ||
        expression_type != return_type) return false;
    int64_t final_count = swan_fn_sil_emit_return(state, count, return_value);
    if (final_count != count + 1) return false;
    count = final_count;

    int64_t optimized = swan_fn_sil_optimize(state, count, workspace);
    if (optimized < 2 || optimized > 256) return false;
    count = optimized;

    SwanIRFunction *ir_fn = swan_ir_add_function(module, fn->name, return_type,
                                                  parameter_types, fn->param_count);
    if (!ir_fn) return false;
    SwanIRBlock *entry = swan_ir_add_block(ir_fn, "entry");
    if (!entry) return false;
    uint32_t values[256] = {0};
    for (size_t i = 0; i < fn->param_count; ++i) values[i + 1] = (uint32_t)i + 1;

    bool returned = false;
    for (int64_t i = 0; i < count; ++i) {
        size_t base = (size_t)i * 5;
        int encoded = (int)state[base];
        int opcode = encoded % 16;
        int type_code = encoded / 16;
        uint32_t destination = state[base + 1];
        uint32_t left = state[base + 2];
        uint32_t right = state[base + 3];
        if (opcode == 11) {
            if (!destination || destination > fn->param_count) return false;
            values[destination] = destination;
            continue;
        }
        if (opcode == 1) {
            if (!destination || destination >= 256 || (type_code == 2 && state[base + 4] > INT32_MAX)) return false;
            SwanIRInst inst = {0};
            inst.op = SWAN_IR_CONST;
            inst.type = swan_sil_ir_type(type_code);
            inst.immediate = (int32_t)state[base + 4];
            values[destination] = swan_ir_add_inst(ir_fn, entry, inst);
            if (!values[destination]) return false;
            continue;
        }
        if (opcode >= 2 && opcode <= 4) {
            if (!destination || destination >= 256 || !values[left] || !values[right]) return false;
            SwanIRInst inst = {0};
            inst.op = opcode == 2 ? SWAN_IR_ADD : opcode == 3 ? SWAN_IR_SUB : SWAN_IR_MUL;
            inst.type = swan_sil_ir_type(type_code);
            inst.a = values[left];
            inst.b = values[right];
            values[destination] = swan_ir_add_inst(ir_fn, entry, inst);
            if (!values[destination]) return false;
            continue;
        }
        if (opcode == 7 || opcode == 8) {
            if (!destination || destination >= 256 || !values[left] || !values[right]) return false;
            SwanIRInst inst = {0};
            inst.op = opcode == 7 ? SWAN_IR_ICMP_EQ : SWAN_IR_ICMP_SLT;
            inst.type = SWAN_IR_I1;
            inst.a = values[left];
            inst.b = values[right];
            values[destination] = swan_ir_add_inst(ir_fn, entry, inst);
            if (!values[destination]) return false;
            continue;
        }
        if (opcode == 10) {
            if (!values[left] || !swan_ir_terminate(entry, (SwanIRTerm){SWAN_IR_RET, values[left], 0, 0})) return false;
            returned = true;
            continue;
        }
        return false;
    }
    return returned;
}

static inline bool swan_sil_try_irgen_program(Program *program, SwanIRModule *output) {
    if (!program || !output || !program->count || program->import_count ||
        program->count != program->source_function_count || program->count > 1024) return false;
    SwanIRModule candidate = {0};
    for (size_t i = 0; i < program->count; ++i) {
        if (!swan_sil_lower_function(program->functions[i], &candidate)) {
            swan_ir_destroy(&candidate);
            return false;
        }
    }
    char verify_error[256] = {0};
    if (!swan_ir_verify(&candidate, verify_error, sizeof(verify_error))) {
        swan_ir_destroy(&candidate);
        return false;
    }
    *output = candidate;
    return true;
}
#endif
