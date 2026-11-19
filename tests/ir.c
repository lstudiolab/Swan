#include "brew/ir.h"
#include <assert.h>
#include <string.h>

int main(void) {
    BrewIRModule m = {0};
    BrewIRType params[] = {BREW_IR_I64};
    BrewIRFunction *f = brew_ir_add_function(&m, "sum", BREW_IR_I64, params, 1);
    assert(f);

    BrewIRBlock *entry = brew_ir_add_block(f, "entry");
    assert(entry);

    BrewIRInst add = {0};
    add.op = BREW_IR_ADD;
    add.type = BREW_IR_I64;
    add.a = 1;
    add.b = 1;
    uint32_t result = brew_ir_add_inst(f, entry, add);
    assert(result == 2);
    assert(brew_ir_value_type_of(f, 1) == BREW_IR_I64);
    assert(brew_ir_value_type_of(f, result) == BREW_IR_I64);
    assert(brew_ir_value_type_of(f, 99) == BREW_IR_VOID);

    BrewIRTerm ret = {BREW_IR_RET, result, 0, 0};
    assert(brew_ir_terminate(entry, ret));

    char error[256];
    assert(brew_ir_verify(&m, error, sizeof(error)));

    entry->terminator.value = 99;
    assert(!brew_ir_verify(&m, error, sizeof(error)));
    assert(strstr(error, "undefined value"));

    entry->terminator.value = result;
    assert(brew_ir_verify(&m, error, sizeof(error)));

    entry->instructions[0].a = result;
    assert(!brew_ir_verify(&m, error, sizeof(error)));
    assert(strstr(error, "before its definition"));

    entry->instructions[0].a = 1;
    entry->instructions[0].type = BREW_IR_I64;
    assert(brew_ir_verify(&m, error, sizeof(error)));

    entry->instructions[0].type = BREW_IR_I32;
    assert(!brew_ir_verify(&m, error, sizeof(error)));
    assert(strstr(error, "operand type mismatch"));
    entry->instructions[0].type = BREW_IR_I64;
    assert(brew_ir_verify(&m, error, sizeof(error)));

    /* Loop-carried PHI: the backedge comes from a later-numbered block. */
    BrewIRType loop_params[] = {BREW_IR_I64, BREW_IR_I1};
    BrewIRFunction *loop = brew_ir_add_function(&m, "loop_phi", BREW_IR_I64, loop_params, 2);
    assert(loop);
    BrewIRBlock *loop_entry = brew_ir_add_block(loop, "entry");
    BrewIRBlock *header = brew_ir_add_block(loop, "header");
    BrewIRBlock *body = brew_ir_add_block(loop, "body");
    BrewIRBlock *exit = brew_ir_add_block(loop, "exit");
    assert(loop_entry && header && body && exit);
    assert(brew_ir_terminate(loop_entry, (BrewIRTerm){BREW_IR_BR, 0, 1, 0}));
    BrewIRIncoming incoming[] = {{1, 0}, {4, 2}};
    BrewIRInst phi = {0};
    phi.op = BREW_IR_PHI;
    phi.type = BREW_IR_I64;
    phi.incoming = malloc(sizeof(incoming));
    assert(phi.incoming);
    memcpy(phi.incoming, incoming, sizeof(incoming));
    phi.incoming_count = 2;
    assert(brew_ir_add_inst(loop, header, phi) == 3);
    assert(brew_ir_terminate(header, (BrewIRTerm){BREW_IR_COND_BR, 2, 2, 3}));
    BrewIRInst increment = {0};
    increment.op = BREW_IR_ADD;
    increment.type = BREW_IR_I64;
    increment.a = 3;
    increment.b = 1;
    assert(brew_ir_add_inst(loop, body, increment) == 4);
    assert(brew_ir_terminate(body, (BrewIRTerm){BREW_IR_BR, 0, 1, 0}));
    assert(brew_ir_terminate(exit, (BrewIRTerm){BREW_IR_RET, 3, 0, 0}));
    if (!brew_ir_verify(&m, error, sizeof(error))) fprintf(stderr, "loop PHI verifier: %s\n", error);
    assert(brew_ir_verify(&m, error, sizeof(error)));

    brew_ir_destroy(&m);
    return 0;
}
