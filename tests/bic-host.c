#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "../compiler/lib/Driver/BIC.h"

int main(void) {
    BrewSICHost host = brew_sic_detect_host();
#if defined(__wasm__) || defined(__wasm64__) || defined(__wasi__)
    return host.environment == BREW_SIC_ENV_WEBASSEMBLY ? 0 : 1;
#else
    if (host.environment != BREW_SIC_ENV_NATIVE) return 1;
    return host.architecture == BREW_SIC_ARCH_X86_64 ||
           host.architecture == BREW_SIC_ARCH_ARM64 ? 0 : 1;
#endif
}
