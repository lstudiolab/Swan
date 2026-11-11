#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define IR_INSTRUCTIONS 256
#define CONSTANTS 128
#define ROUNDS 128
extern int64_t swan_fn_swanc_ir_optimize(uint32_t *, int64_t, uint32_t *, uint32_t *, int64_t, uint32_t *);
int64_t swan_optimizer_test_main(void) {
    uint32_t instructions[IR_INSTRUCTIONS * 8];
    uint32_t known[IR_INSTRUCTIONS + 1], values[IR_INSTRUCTIONS + 1], diagnostic[2];
    for (int round = 0; round < ROUNDS; ++round) {
        memset(instructions, 0, sizeof(instructions));
        memset(known, 0, sizeof(known));
        memset(values, 0, sizeof(values));
        memset(diagnostic, 0, sizeof(diagnostic));
        for (uint32_t i = 0; i < CONSTANTS; ++i) {
            uint32_t base = i * 8;
            instructions[base] = 0;
            instructions[base + 1] = 4;
            instructions[base + 2] = i + 1;
            instructions[base + 6] = i + 1;
        }
        for (uint32_t i = 0; i < CONSTANTS; ++i) {
            uint32_t base = (CONSTANTS + i) * 8;
            instructions[base] = 1;
            instructions[base + 1] = 4;
            instructions[base + 2] = CONSTANTS + i + 1;
            instructions[base + 3] = i * 2 + 1;
            instructions[base + 4] = i * 2 + 2;
        }
        int64_t status = swan_fn_swanc_ir_optimize(instructions, IR_INSTRUCTIONS, known, values, IR_INSTRUCTIONS + 1, diagnostic);
        if (status != 0) {
            fprintf(stderr, "optimizer failed: status=%lld diagnostic=%u at instruction=%u\n", (long long)status, diagnostic[0], diagnostic[1]);
            return 1;
        }
        for (uint32_t i = 0; i < CONSTANTS; ++i) {
            uint32_t base = (CONSTANTS + i) * 8;
            uint32_t expected = (i * 2 + 1) + (i * 2 + 2);
            if (instructions[base] != 0 || instructions[base + 6] != expected) {
                fprintf(stderr, "incorrect fold at round=%d instruction=%u\n", round, i);
                return 2;
            }
        }
    }
    puts("Compiler 2 optimizer correctness passed: 128 rounds, 32,768 IR instructions, 16,384 folds.");
    return 0;
}
