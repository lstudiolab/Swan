#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "../compiler/lib/Driver/SIC.h"

int main(void) {
    SwanSICHost host = swan_sic_detect_host();
#if defined(__wasm__) || defined(__wasm64__) || defined(__wasi__)
    return host.environment == SWAN_SIC_ENV_WEBASSEMBLY ? 0 : 1;
#else
    if (host.environment != SWAN_SIC_ENV_NATIVE) return 1;
    return host.architecture == SWAN_SIC_ARCH_X86_64 ||
           host.architecture == SWAN_SIC_ARCH_ARM64 ? 0 : 1;
#endif
}
