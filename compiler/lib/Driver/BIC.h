#ifndef BREW_BIC_H
#define BREW_BIC_H

#include <sys/utsname.h>
#include <string.h>

/*
 * BIC (Brew Integrated Compilation) coordinates the portable WASM artifact
 * with the execution environment. It is not a bytecode format or a second
 * source-language compiler.
 */
typedef enum {
    BREW_BIC_ENV_NATIVE,
    BREW_BIC_ENV_WEBASSEMBLY
} BrewBICEnvironment;

typedef enum {
    BREW_BIC_ARCH_UNKNOWN,
    BREW_BIC_ARCH_X86_64,
    BREW_BIC_ARCH_ARM64
} BrewBICArchitecture;

typedef struct {
    BrewBICEnvironment environment;
    BrewBICArchitecture architecture;
} BrewBICHost;

static inline BrewBICHost brew_sic_detect_host(void) {
    BrewBICHost host = {BREW_BIC_ENV_NATIVE, BREW_BIC_ARCH_UNKNOWN};
#if defined(__wasm__) || defined(__wasm64__) || defined(__wasi__)
    host.environment = BREW_BIC_ENV_WEBASSEMBLY;
#else
    struct utsname info;
    if (uname(&info) != 0) return host;
    if (!strcmp(info.machine, "x86_64") || !strcmp(info.machine, "amd64"))
        host.architecture = BREW_BIC_ARCH_X86_64;
    else if (!strcmp(info.machine, "aarch64") || !strcmp(info.machine, "arm64"))
        host.architecture = BREW_BIC_ARCH_ARM64;
#endif
    return host;
}

#endif
