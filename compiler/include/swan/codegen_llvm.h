#ifndef SWAN_CODEGEN_LLVM_H
#define SWAN_CODEGEN_LLVM_H
#include "swan/ir.h"
#include "swan/codegen_x86_64.h"

/* Optional portability backend. Swan still owns parsing, typed IR and
 * optimization; LLVM IR is the interchange layer used only when a target
 * is outside Swan's hand-written native backend matrix. */
static inline const char *swan_llvm_type(SwanIRType type) {
    switch (type) {
        case SWAN_IR_VOID: return "void";
        case SWAN_IR_I1: return "i1";
        case SWAN_IR_I8: return "i8";
        case SWAN_IR_I16: return "i16";
        case SWAN_IR_I32: return "i32";
        case SWAN_IR_I64: return "i64";
        case SWAN_IR_F32: return "float";
        case SWAN_IR_F64: return "double";
        case SWAN_IR_PTR: return "ptr";
        default: return NULL;
    }
}
static inline bool swan_llvm_name_valid(const char *name) {
    if (!name || !((*name >= 'A' && *name <= 'Z') ||
                   (*name >= 'a' && *name <= 'z') || *name == '_' || *name == '$' || *name == '.'))
        return false;
    for (const char *p = name + 1; *p; ++p)
        if (!( (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
               (*p >= '0' && *p <= '9') || *p == '_' || *p == '$' || *p == '.' || *p == '-'))
            return false;
    return true;
}
static inline bool swan_llvm_emit_signature(Text *out, const SwanIRFunction *fn, bool declaration) {
    const char *ret = swan_llvm_type(fn->return_type);
    if (!ret || !swan_llvm_name_valid(fn->name)) return false;
    text_add(out, "%s %s @%s(", declaration ? "declare" : "define", ret, fn->name);
    for (size_t p = 0; p < fn->parameter_count; ++p) {
        const char *type = swan_llvm_type(fn->parameter_types[p]);
        if (!type) return false;
        if (p) text_add(out, ", ");
        text_add(out, "%s", type);
        if (!declaration) text_add(out, " %%v%zu", p + 1);
    }
    text_add(out, ")");
    if (!declaration) text_add(out, " {\n");
    else text_add(out, "\n");
    return !out->failed;
}
static inline bool swan_llvm_emit_inst(Text *out, const SwanIRFunction *fn, const SwanIRInst *in) {
    const char *type = swan_llvm_type(in->type);
    const char *operand = swan_llvm_type(swan_ir_value_type_of(fn, in->a));
    if (in->op == SWAN_IR_PHI) {
        if (!type || !in->incoming_count) return false;
        text_add(out, "  %%v%u = phi %s ", in->result, type);
        for (size_t k = 0; k < in->incoming_count; ++k) {
            if (k) text_add(out, ", ");
            text_add(out, "[ %%v%u, %%bb%u ]", in->incoming[k].value, in->incoming[k].block);
        }
        text_add(out, "\n");
        return !out->failed;
    }
    if (in->op == SWAN_IR_CONST) {
        if (!type) return false;
        if (in->type == SWAN_IR_PTR) text_add(out, "  %%v%u = inttoptr i64 %lld to ptr\n", in->result, (long long)in->immediate);
        else text_add(out, "  %%v%u = add %s 0, %lld\n", in->result, type, (long long)in->immediate);
        return !out->failed;
    }
    if (in->op == SWAN_IR_ALLOCA) {
        text_add(out, "  %%v%u = alloca i8, align 8\n", in->result);
        return !out->failed;
    }
    if (in->op == SWAN_IR_LOAD) {
        if (!type) return false;
        text_add(out, "  %%v%u = load %s, ptr %%v%u, align %u\n",
                 in->result, type, in->a,
                 in->type == SWAN_IR_I8 || in->type == SWAN_IR_I1 ? 1u :
                 in->type == SWAN_IR_I16 ? 2u :
                 in->type == SWAN_IR_I32 || in->type == SWAN_IR_F32 ? 4u : 8u);
        return !out->failed;
    }
    if (in->op == SWAN_IR_STORE) {
        SwanIRType stored = swan_ir_value_type_of(fn, in->b);
        const char *stored_type = swan_llvm_type(stored);
        if (!stored_type) return false;
        text_add(out, "  store %s %%v%u, ptr %%v%u, align %u\n",
                 stored_type, in->b, in->a,
                 stored == SWAN_IR_I8 || stored == SWAN_IR_I1 ? 1u :
                 stored == SWAN_IR_I16 ? 2u :
                 stored == SWAN_IR_I32 || stored == SWAN_IR_F32 ? 4u : 8u);
        return !out->failed;
    }
    if (in->op == SWAN_IR_CALL) {
        if (!type || !swan_llvm_name_valid(in->symbol)) return false;
        if (in->type != SWAN_IR_VOID) text_add(out, "  %%v%u = ", in->result);
        else text_add(out, "  ");
        text_add(out, "call %s @%s(", type, in->symbol);
        for (size_t k = 0; k < in->incoming_count; ++k) {
            uint32_t id = in->incoming[k].value;
            const char *arg_type = swan_llvm_type(swan_ir_value_type_of(fn, id));
            if (!arg_type) return false;
            if (k) text_add(out, ", ");
            text_add(out, "%s %%v%u", arg_type, id);
        }
        text_add(out, ")\n");
        return !out->failed;
    }
    if (in->op == SWAN_IR_SELECT) {
        text_add(out, "  %%v%u = select i1 %%v%u, %s %%v%u, %s %%v%u\n",
                 in->result, in->a, type, in->b, type, in->c);
        return !out->failed;
    }
    if (!operand || !type) return false;
    const char *op = NULL;
    switch (in->op) {
        case SWAN_IR_ADD: op = "add"; break;
        case SWAN_IR_SUB: op = "sub"; break;
        case SWAN_IR_MUL: op = "mul"; break;
        case SWAN_IR_SDIV: op = "sdiv"; break;
        case SWAN_IR_UDIV: op = "udiv"; break;
        case SWAN_IR_SREM: op = "srem"; break;
        case SWAN_IR_UREM: op = "urem"; break;
        case SWAN_IR_AND: op = "and"; break;
        case SWAN_IR_OR: op = "or"; break;
        case SWAN_IR_XOR: op = "xor"; break;
        case SWAN_IR_SHL: op = "shl"; break;
        case SWAN_IR_LSHR: op = "lshr"; break;
        case SWAN_IR_ASHR: op = "ashr"; break;
        case SWAN_IR_ICMP_EQ: op = "icmp eq"; break;
        case SWAN_IR_ICMP_NE: op = "icmp ne"; break;
        case SWAN_IR_ICMP_SLT: op = "icmp slt"; break;
        case SWAN_IR_ICMP_SLE: op = "icmp sle"; break;
        case SWAN_IR_ICMP_SGT: op = "icmp sgt"; break;
        case SWAN_IR_ICMP_SGE: op = "icmp sge"; break;
        default: return false;
    }
    text_add(out, "  %%v%u = %s %s %%v%u, %%v%u\n",
             in->result, op, operand, in->a, in->b);
    return !out->failed;
}
static inline bool swan_codegen_llvm_ir(const SwanIRModule *module, Text *out,
                                         const char *triple, char *error, size_t cap) {
    if (!swan_ir_verify(module, error, cap)) return false;
    if (!triple || !*triple) {
        if (error && cap) snprintf(error, cap, "LLVM target triple is empty");
        return false;
    }
    for (const char *p = triple; *p; ++p) {
        if (!( (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
               (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.')) {
            if (error && cap) snprintf(error, cap, "invalid character in target triple");
            return false;
        }
    }
    text_add(out, "source_filename = \"swan\"\ntarget triple = \"%s\"\n\n", triple);
    for (size_t f = 0; f < module->function_count; ++f) {
        const SwanIRFunction *fn = &module->functions[f];
        if (fn->external && !swan_llvm_emit_signature(out, fn, true)) goto unsupported;
    }
    if (module->function_count) text_add(out, "\n");
    for (size_t f = 0; f < module->function_count; ++f) {
        const SwanIRFunction *fn = &module->functions[f];
        if (fn->external) continue;
        if (!swan_llvm_emit_signature(out, fn, false)) goto unsupported;
        for (size_t b = 0; b < fn->block_count; ++b) {
            const SwanIRBlock *block = &fn->blocks[b];
            text_add(out, "bb%zu:\n", b);
            for (size_t i = 0; i < block->instruction_count; ++i)
                if (!swan_llvm_emit_inst(out, fn, &block->instructions[i])) goto unsupported;
            const SwanIRTerm *term = &block->terminator;
            if (term->kind == SWAN_IR_BR) text_add(out, "  br label %%bb%u\n", term->target);
            else if (term->kind == SWAN_IR_COND_BR)
                text_add(out, "  br i1 %%v%u, label %%bb%u, label %%bb%u\n",
                         term->value, term->target, term->alternate);
            else if (term->kind == SWAN_IR_RET) {
                if (term->value) {
                    const char *ret = swan_llvm_type(fn->return_type);
                    if (!ret) goto unsupported;
                    text_add(out, "  ret %s %%v%u\n", ret, term->value);
                } else text_add(out, "  ret void\n");
            } else if (term->kind == SWAN_IR_UNREACHABLE) text_add(out, "  unreachable\n");
            else goto unsupported;
        }
        text_add(out, "}\n\n");
        if (out->failed) goto unsupported;
    }
    if (error && cap) error[0] = 0;
    return true;
unsupported:
    if (error && cap) snprintf(error, cap, "LLVM IR emission failed or encountered an unsupported IR construct");
    return false;
}
#endif
