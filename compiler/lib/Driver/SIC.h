#ifndef BREW_SIC_H
#define BREW_SIC_H

#include <sys/utsname.h>
#include <string.h>

/*
 * SIC (Brew Integrated Compilation) coordinates the portable WASM artifact
 * with the execution environment. It is not a bytecode format or a second
 * source-language compiler.
 */
typedef enum {
    BREW_SIC_ENV_NATIVE,
    BREW_SIC_ENV_WEBASSEMBLY
} BrewSICEnvironment;

typedef enum {
    BREW_SIC_ARCH_UNKNOWN,
    BREW_SIC_ARCH_X86_64,
    BREW_SIC_ARCH_ARM64
} BrewSICArchitecture;

typedef struct {
    BrewSICEnvironment environment;
    BrewSICArchitecture architecture;
} BrewSICHost;

static inline BrewSICHost brew_sic_detect_host(void) {
    BrewSICHost host = {BREW_SIC_ENV_NATIVE, BREW_SIC_ARCH_UNKNOWN};
#if defined(__wasm__) || defined(__wasm64__) || defined(__wasi__)
    host.environment = BREW_SIC_ENV_WEBASSEMBLY;
#else
    struct utsname info;
    if (uname(&info) != 0) return host;
    if (!strcmp(info.machine, "x86_64") || !strcmp(info.machine, "amd64"))
        host.architecture = BREW_SIC_ARCH_X86_64;
    else if (!strcmp(info.machine, "aarch64") || !strcmp(info.machine, "arm64"))
        host.architecture = BREW_SIC_ARCH_ARM64;
#endif
    return host;
}

#endif
