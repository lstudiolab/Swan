#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IR_INSTRUCTIONS 256
#define CONSTANTS 128
#define ROUNDS 128
#define WORKSPACE_WORDS (2 * (IR_INSTRUCTIONS + 1) + 2)

extern int64_t swan_fn_swanc_ir_optimize(
    uint32_t *instructions,
    int64_t instruction_count,
    uint32_t *workspace,
    int64_t capacity
);

int main(void) {
    uint32_t instructions[IR_INSTRUCTIONS * 8];
    uint32_t workspace[WORKSPACE_WORDS];

    for (int round = 0; round < ROUNDS; ++round) {
        memset(instructions, 0, sizeof(instructions));
        memset(workspace, 0, sizeof(workspace));

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

        int64_t status = swan_fn_swanc_ir_optimize(
            instructions, IR_INSTRUCTIONS, workspace, IR_INSTRUCTIONS + 1
        );
        if (status != 0) {
            fprintf(stderr, "optimizer failed: status=%lld diagnostic=%u at instruction=%u\n",
                    (long long)status, workspace[0], workspace[1]);
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
