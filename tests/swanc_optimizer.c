#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IR_INSTRUCTIONS 515
#define CONSTANTS 256
#define ADDITIONS 128
#define ROUNDS 128
#define WORKSPACE_WORDS (3 * (IR_INSTRUCTIONS + 1) + 2)

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

        instructions[0] = 22;
        instructions[1] = 8;
        instructions[2] = 1;

        for (uint32_t i = 0; i < CONSTANTS; ++i) {
            uint32_t base = (i + 1) * 8;
            instructions[base] = 0;
            instructions[base + 1] = 4;
            instructions[base + 2] = i + 2;
            instructions[base + 6] = i + 1;
        }
        for (uint32_t i = 0; i < ADDITIONS; ++i) {
            uint32_t base = (1 + CONSTANTS + i) * 8;
            instructions[base] = 1;
            instructions[base + 1] = 4;
            instructions[base + 2] = CONSTANTS + i + 2;
            instructions[base + 3] = i * 2 + 2;
            instructions[base + 4] = i * 2 + 3;
        }
        for (uint32_t i = 0; i < ADDITIONS; ++i) {
            uint32_t base = (1 + CONSTANTS + ADDITIONS + i) * 8;
            instructions[base] = 24;
            instructions[base + 3] = 1;
            instructions[base + 4] = CONSTANTS + i + 2;
        }

        uint32_t load_base = (1 + CONSTANTS + ADDITIONS * 2) * 8;
        instructions[load_base] = 23;
        instructions[load_base + 1] = 4;
        instructions[load_base + 2] = CONSTANTS + ADDITIONS + 2;
        instructions[load_base + 3] = 1;

        uint32_t dead_base = (1 + CONSTANTS + ADDITIONS * 2 + 1) * 8;
        instructions[dead_base] = 1;
        instructions[dead_base + 1] = 4;
        instructions[dead_base + 2] = CONSTANTS + ADDITIONS + 3;
        instructions[dead_base + 3] = CONSTANTS + ADDITIONS + 2;
        instructions[dead_base + 4] = 2;

        int64_t status = swan_fn_swanc_ir_optimize(
            instructions, IR_INSTRUCTIONS, workspace, IR_INSTRUCTIONS + 1
        );
        if (status != 0) {
            fprintf(stderr, "optimizer failed: status=%lld diagnostic=%u at instruction=%u\n",
                    (long long)status, workspace[0], workspace[1]);
            return 1;
        }
        for (uint32_t i = 0; i < ADDITIONS; ++i) {
            uint32_t base = (1 + CONSTANTS + i) * 8;
            uint32_t expected = (i * 2 + 1) + (i * 2 + 2);
            if (instructions[base] != 0 || instructions[base + 6] != expected) {
                fprintf(stderr, "incorrect fold at round=%d instruction=%u\n", round, i);
                return 2;
            }
        }
        if (instructions[dead_base] != 0 || instructions[dead_base + 6] != 0) {
            fprintf(stderr, "dead instruction survived DCE at round=%d\n", round);
            return 3;
        }
        if (instructions[load_base] != 23) {
            fprintf(stderr, "DCE removed a potentially trapping load at round=%d\n", round);
            return 4;
        }
    }
    puts("Compiler 2 optimizer correctness passed: 128 rounds, 65,920 IR instructions, 16,384 folds, 128 dead instructions removed.");
    return 0;
}
