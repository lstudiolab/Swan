#ifndef SWAN_TARGET_H
#define SWAN_TARGET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    SWAN_ARCH_X86_64,
    SWAN_ARCH_AARCH64,
    SWAN_ARCH_ARMV7,
    SWAN_ARCH_RISCV64,
    SWAN_ARCH_WASM32,
    SWAN_ARCH_WASM64,
    SWAN_ARCH_SVM
} SwanArchitecture;

typedef enum {
    SWAN_OBJECT_ELF,
    SWAN_OBJECT_WASM
} SwanObjectFormat;

typedef enum {
    SWAN_ABI_SYSV,
    SWAN_ABI_AAPCS64,
    SWAN_ABI_AAPCS32,
    SWAN_ABI_RISCV,
    SWAN_ABI_WASM
} SwanABI;

typedef struct {
    const char *name;
    const char *triple;
    SwanArchitecture architecture;
    SwanObjectFormat object_format;
    SwanABI abi;
    bool backend_available;
} SwanTarget;

static const SwanTarget swan_targets[] = {
    {"x86_64", "x86_64-unknown-linux-gnu", SWAN_ARCH_X86_64, SWAN_OBJECT_ELF, SWAN_ABI_SYSV, true},
    {"aarch64", "aarch64-unknown-linux-gnu", SWAN_ARCH_AARCH64, SWAN_OBJECT_ELF, SWAN_ABI_AAPCS64, true},
    {"armv7", "armv7-unknown-linux-gnueabihf", SWAN_ARCH_ARMV7, SWAN_OBJECT_ELF, SWAN_ABI_AAPCS32, false},
    {"riscv64", "riscv64-unknown-linux-gnu", SWAN_ARCH_RISCV64, SWAN_OBJECT_ELF, SWAN_ABI_RISCV, false},
    {"wasm32", "wasm32-unknown-unknown", SWAN_ARCH_WASM32, SWAN_OBJECT_WASM, SWAN_ABI_WASM, true},
    {"wasm64", "wasm64-unknown-unknown", SWAN_ARCH_WASM64, SWAN_OBJECT_WASM, SWAN_ABI_WASM, false},
    {"svm", "swanvm-unknown-portable", SWAN_ARCH_SVM, SWAN_OBJECT_WASM, SWAN_ABI_WASM, true}
};

#define SWAN_TARGET_COUNT (sizeof(swan_targets) / sizeof(swan_targets[0]))

static inline const SwanTarget *swan_target_find(const char *name) {
    if (!name) return NULL;
    if (!strcmp(name, "x86-64") || !strcmp(name, "amd64")) name = "x86_64";
    else if (!strcmp(name, "arm64")) name = "aarch64";
    else if (!strcmp(name, "arm")) name = "armv7";
    else if (!strcmp(name, "riscv")) name = "riscv64";
    for (size_t i = 0; i < SWAN_TARGET_COUNT; ++i)
        if (!strcmp(name, swan_targets[i].name) || !strcmp(name, swan_targets[i].triple))
            return &swan_targets[i];
    return NULL;
}

static inline void swan_target_list(FILE *out) {
    for (size_t i = 0; i < SWAN_TARGET_COUNT; ++i)
        fprintf(out, "%-9s %-32s %s\n", swan_targets[i].name,
                swan_targets[i].triple,
                swan_targets[i].backend_available ? "implemented" : "planned");
}

#endif
