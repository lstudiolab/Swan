#include "swan/ir.h"
#include <assert.h>
#include <string.h>

int main(void) {
    SwanIRModule m = {0};
    SwanIRType params[] = {SWAN_IR_I64};
    SwanIRFunction *f = swan_ir_add_function(&m, "sum", SWAN_IR_I64, params, 1);
    assert(f);

    SwanIRBlock *entry = swan_ir_add_block(f, "entry");
    assert(entry);

    SwanIRInst add = {0};
    add.op = SWAN_IR_ADD;
    add.type = SWAN_IR_I64;
    add.a = 1;
    add.b = 1;
    uint32_t result = swan_ir_add_inst(f, entry, add);
    assert(result == 2);
    assert(swan_ir_value_type_of(f, 1) == SWAN_IR_I64);
    assert(swan_ir_value_type_of(f, result) == SWAN_IR_I64);
    assert(swan_ir_value_type_of(f, 99) == SWAN_IR_VOID);

    SwanIRTerm ret = {SWAN_IR_RET, result, 0, 0};
    assert(swan_ir_terminate(entry, ret));

    char error[256];
    assert(swan_ir_verify(&m, error, sizeof(error)));

    entry->terminator.value = 99;
    assert(!swan_ir_verify(&m, error, sizeof(error)));
    assert(strstr(error, "undefined value"));

    entry->terminator.value = result;
    assert(swan_ir_verify(&m, error, sizeof(error)));

    entry->instructions[0].a = result;
    assert(!swan_ir_verify(&m, error, sizeof(error)));
    assert(strstr(error, "before its definition"));

    entry->instructions[0].a = 1;
    entry->instructions[0].type = SWAN_IR_I64;
    assert(swan_ir_verify(&m, error, sizeof(error)));

    entry->instructions[0].type = SWAN_IR_I32;
    assert(!swan_ir_verify(&m, error, sizeof(error)));
    assert(strstr(error, "operand type mismatch"));
    entry->instructions[0].type = SWAN_IR_I64;
    assert(swan_ir_verify(&m, error, sizeof(error)));

    /* Loop-carried PHI: the backedge comes from a later-numbered block. */
    SwanIRType loop_params[] = {SWAN_IR_I64, SWAN_IR_I1};
    SwanIRFunction *loop = swan_ir_add_function(&m, "loop_phi", SWAN_IR_I64, loop_params, 2);
    assert(loop);
    SwanIRBlock *loop_entry = swan_ir_add_block(loop, "entry");
    SwanIRBlock *header = swan_ir_add_block(loop, "header");
    SwanIRBlock *body = swan_ir_add_block(loop, "body");
    SwanIRBlock *exit = swan_ir_add_block(loop, "exit");
    assert(loop_entry && header && body && exit);
    assert(swan_ir_terminate(loop_entry, (SwanIRTerm){SWAN_IR_BR, 0, 1, 0}));
    SwanIRIncoming incoming[] = {{1, 0}, {4, 2}};
    SwanIRInst phi = {0};
    phi.op = SWAN_IR_PHI;
    phi.type = SWAN_IR_I64;
    phi.incoming = malloc(sizeof(incoming));
    assert(phi.incoming);
    memcpy(phi.incoming, incoming, sizeof(incoming));
    phi.incoming_count = 2;
    assert(swan_ir_add_inst(loop, header, phi) == 3);
    assert(swan_ir_terminate(header, (SwanIRTerm){SWAN_IR_COND_BR, 2, 2, 3}));
    SwanIRInst increment = {0};
    increment.op = SWAN_IR_ADD;
    increment.type = SWAN_IR_I64;
    increment.a = 3;
    increment.b = 1;
    assert(swan_ir_add_inst(loop, body, increment) == 4);
    assert(swan_ir_terminate(body, (SwanIRTerm){SWAN_IR_BR, 0, 1, 0}));
    assert(swan_ir_terminate(exit, (SwanIRTerm){SWAN_IR_RET, 3, 0, 0}));
    if (!swan_ir_verify(&m, error, sizeof(error))) fprintf(stderr, "loop PHI verifier: %s\n", error);
    assert(swan_ir_verify(&m, error, sizeof(error)));

    swan_ir_destroy(&m);
    return 0;
}
