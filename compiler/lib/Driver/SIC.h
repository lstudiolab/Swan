#ifndef SWAN_SIC_H
#define SWAN_SIC_H

#include <sys/utsname.h>

/*
 * SIC (Swan Integrated Compilation) coordinates the portable WASM artifact
 * with the execution environment. It is not a bytecode format or a second
 * source-language compiler.
 */
typedef enum {
    SWAN_SIC_ENV_NATIVE,
    SWAN_SIC_ENV_WEBASSEMBLY
} SwanSICEnvironment;

typedef enum {
    SWAN_SIC_ARCH_UNKNOWN,
    SWAN_SIC_ARCH_X86_64,
    SWAN_SIC_ARCH_ARM64
} SwanSICArchitecture;

typedef struct {
    SwanSICEnvironment environment;
    SwanSICArchitecture architecture;
} SwanSICHost;

static inline SwanSICHost swan_sic_detect_host(void) {
    SwanSICHost host = {SWAN_SIC_ENV_NATIVE, SWAN_SIC_ARCH_UNKNOWN};
#if defined(__wasm__) || defined(__wasm64__) || defined(__wasi__)
    host.environment = SWAN_SIC_ENV_WEBASSEMBLY;
#else
    struct utsname info;
    if (uname(&info) != 0) return host;
    if (!strcmp(info.machine, "x86_64") || !strcmp(info.machine, "amd64"))
        host.architecture = SWAN_SIC_ARCH_X86_64;
    else if (!strcmp(info.machine, "aarch64") || !strcmp(info.machine, "arm64"))
        host.architecture = SWAN_SIC_ARCH_ARM64;
#endif
    return host;
}

#endif
