#ifndef SWAN_SVM_H
#define SWAN_SVM_H
#include "swan/ir.h"
#include <limits.h>

/* SVM is Swan's portable, register-based virtual machine. Its instruction
 * operands are SSA value IDs, so the same verified IR is executable on any
 * host CPU without a host-specific instruction selector. */
#define SWAN_SVM_MAGIC "SWANVM01"
#define SWAN_SVM_VERSION 1u
#define SWAN_SVM_MAX_CALL_DEPTH 256u

typedef struct {
    const SwanIRModule *module;
    uint64_t fuel;
    unsigned depth;
    char *error;
    size_t error_capacity;
} SwanVM;

static inline bool swan_svm_fail(SwanVM *vm, const char *message) {
    if (vm->error && vm->error_capacity)
        snprintf(vm->error, vm->error_capacity, "%s", message);
    return false;
}

static inline bool swan_svm_eval(SwanVM *vm, size_t function_index,
                                 const int64_t *arguments, size_t argument_count,
                                 int64_t *result) {
    if (function_index >= vm->module->function_count)
        return swan_svm_fail(vm, "SVM call references an invalid function");
    if (vm->depth >= SWAN_SVM_MAX_CALL_DEPTH)
        return swan_svm_fail(vm, "SVM maximum call depth exceeded");
    const SwanIRFunction *fn = &vm->module->functions[function_index];
    if (argument_count != fn->parameter_count)
        return swan_svm_fail(vm, "SVM function argument count mismatch");
    size_t value_count = (size_t)fn->next_value + 1;
    int64_t *values = calloc(value_count, sizeof(*values));
    if (!values) return swan_svm_fail(vm, "SVM out of memory allocating registers");
    for (size_t i = 0; i < argument_count; ++i) values[i + 1] = arguments[i];
    ++vm->depth;
    size_t block_index = 0, predecessor = SIZE_MAX;
    bool returned = false, ok = true;
    while (ok && !returned) {
        if (block_index >= fn->block_count) { ok = swan_svm_fail(vm, "SVM branch target out of range"); break; }
        const SwanIRBlock *block = &fn->blocks[block_index];
        for (size_t i = 0; i < block->instruction_count && ok; ++i) {
            if (vm->fuel == 0) { ok = swan_svm_fail(vm, "SVM execution fuel exhausted"); break; }
            --vm->fuel;
            const SwanIRInst *in = &block->instructions[i];
            int64_t a = in->a < value_count ? values[in->a] : 0;
            int64_t b = in->b < value_count ? values[in->b] : 0;
            int64_t v = 0;
            switch (in->op) {
                case SWAN_IR_CONST: v = in->immediate; break;
                case SWAN_IR_ADD: v = (int64_t)((uint64_t)a + (uint64_t)b); break;
                case SWAN_IR_SUB: v = (int64_t)((uint64_t)a - (uint64_t)b); break;
                case SWAN_IR_MUL: v = (int64_t)((uint64_t)a * (uint64_t)b); break;
                case SWAN_IR_SDIV:
                    if (!b || (a == INT64_MIN && b == -1)) { ok = swan_svm_fail(vm, "SVM signed division trap"); break; }
                    v = a / b; break;
                case SWAN_IR_UDIV:
                    if (!b) { ok = swan_svm_fail(vm, "SVM unsigned division by zero"); break; }
                    v = (int64_t)((uint64_t)a / (uint64_t)b); break;
                case SWAN_IR_SREM:
                    if (!b || (a == INT64_MIN && b == -1)) { ok = swan_svm_fail(vm, "SVM signed remainder trap"); break; }
                    v = a % b; break;
                case SWAN_IR_UREM:
                    if (!b) { ok = swan_svm_fail(vm, "SVM unsigned remainder by zero"); break; }
                    v = (int64_t)((uint64_t)a % (uint64_t)b); break;
                case SWAN_IR_AND: v = a & b; break;
                case SWAN_IR_OR: v = a | b; break;
                case SWAN_IR_XOR: v = a ^ b; break;
                case SWAN_IR_SHL: v = (int64_t)((uint64_t)a << ((uint64_t)b & 63u)); break;
                case SWAN_IR_LSHR: v = (int64_t)((uint64_t)a >> ((uint64_t)b & 63u)); break;
                case SWAN_IR_ASHR: v = a >> ((uint64_t)b & 63u); break;
                case SWAN_IR_ICMP_EQ: v = a == b; break;
                case SWAN_IR_ICMP_NE: v = a != b; break;
                case SWAN_IR_ICMP_SLT: v = a < b; break;
                case SWAN_IR_ICMP_SLE: v = a <= b; break;
                case SWAN_IR_ICMP_SGT: v = a > b; break;
                case SWAN_IR_ICMP_SGE: v = a >= b; break;
                case SWAN_IR_SELECT: v = a ? values[in->b] : values[in->c]; break;
                case SWAN_IR_PHI: {
                    bool found = false;
                    for (size_t k = 0; k < in->incoming_count; ++k)
                        if (in->incoming[k].block == predecessor) {
                            uint32_t id = in->incoming[k].value;
                            if (id >= value_count) { ok = swan_svm_fail(vm, "SVM PHI value out of range"); }
                            else v = values[id];
                            found = true; break;
                        }
                    if (!found) ok = swan_svm_fail(vm, "SVM PHI has no incoming value for predecessor");
                    break;
                }
                case SWAN_IR_CALL: {
                    size_t callee = vm->module->function_count;
                    for (size_t k = 0; k < vm->module->function_count; ++k)
                        if (!strcmp(vm->module->functions[k].name, in->symbol)) { callee = k; break; }
                    if (callee == vm->module->function_count) { ok = swan_svm_fail(vm, "SVM unresolved function call"); break; }
                    if (in->incoming_count > 16) { ok = swan_svm_fail(vm, "SVM call has too many arguments"); break; }
                    int64_t args[16] = {0};
                    for (size_t k = 0; k < in->incoming_count; ++k) {
                        uint32_t id = in->incoming[k].value;
                        if (id >= value_count) { ok = swan_svm_fail(vm, "SVM call argument out of range"); break; }
                        args[k] = values[id];
                    }
                    if (ok && !swan_svm_eval(vm, callee, args, in->incoming_count, &v)) ok = false;
                    break;
                }
                case SWAN_IR_ALLOCA: case SWAN_IR_LOAD: case SWAN_IR_STORE:
                    ok = swan_svm_fail(vm, "SVM memory instructions are not supported by this runtime profile"); break;
                default: ok = swan_svm_fail(vm, "SVM encountered an unsupported IR opcode"); break;
            }
            if (ok && in->result) {
                if (in->result >= value_count) ok = swan_svm_fail(vm, "SVM destination register out of range");
                else values[in->result] = v;
            }
        }
        if (!ok) break;
        const SwanIRTerm *term = &block->terminator;
        switch (term->kind) {
            case SWAN_IR_BR: predecessor = block_index; block_index = term->target; break;
            case SWAN_IR_COND_BR:
                predecessor = block_index;
                if (term->value >= value_count) ok = swan_svm_fail(vm, "SVM branch condition out of range");
                else block_index = values[term->value] ? term->target : term->alternate;
                break;
            case SWAN_IR_RET:
                if (term->value >= value_count) ok = swan_svm_fail(vm, "SVM return value out of range");
                else if (result) *result = term->value ? values[term->value] : 0;
                returned = true; break;
            case SWAN_IR_UNREACHABLE: ok = swan_svm_fail(vm, "SVM reached an unreachable block"); break;
            default: ok = swan_svm_fail(vm, "SVM encountered an invalid terminator"); break;
        }
    }
    --vm->depth;
    free(values);
    return ok;
}

static inline bool swan_svm_run(const SwanIRModule *module, const char *entry,
                                uint64_t fuel, int64_t *result,
                                char *error, size_t error_capacity) {
    char verify_error[256];
    if (!swan_ir_verify(module, verify_error, sizeof(verify_error))) {
        if (error && error_capacity) snprintf(error, error_capacity, "invalid IR: %s", verify_error);
        return false;
    }
    size_t index = module->function_count;
    for (size_t i = 0; i < module->function_count; ++i)
        if (!strcmp(module->functions[i].name, entry)) { index = i; break; }
    if (index == module->function_count) {
        if (error && error_capacity) snprintf(error, error_capacity, "SVM entry function not found");
        return false;
    }
    SwanVM vm = {module, fuel ? fuel : UINT64_MAX, 0, error, error_capacity};
    return swan_svm_eval(&vm, index, NULL, 0, result);
}

/* Stable, versioned container header for portable SVM modules. The IR payload
 * serializer is deliberately separate so format evolution cannot change VM
 * execution semantics. */
typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t flags;
    uint64_t payload_size;
} SwanSVMHeader;

static inline bool swan_svm_write_header(FILE *out, uint64_t payload_size) {
    SwanSVMHeader header = {{'S','W','A','N','V','M','0','1'},
                            SWAN_SVM_VERSION, 0, payload_size};
    return fwrite(&header, sizeof(header), 1, out) == 1;
}

#endif
