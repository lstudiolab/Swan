#ifndef SWAN_SIC_H
#define SWAN_SIC_H

#include <sys/utsname.h>

/*
 * SIC (Swan Integrated Compilation) coordinates the portable WASM artifact
 * with the host execution environment. It is not a bytecode format or a
 * second source-language compiler.
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

/*
 * Wasmtime's compile command translates the AOT WASM module into a
 * host-specific compiled module using its native code generator. The
 * resulting .cwasm is valid only for a compatible host/engine configuration.
 * Keep the portable WASM input until this operation has succeeded.
 */
static inline bool swan_sic_compile_for_host(const char *wasm_path,
                                              char *compiled_path,
                                              size_t compiled_capacity) {
    if (!wasm_path || !compiled_path || !compiled_capacity) return false;
    SwanSICHost host = swan_sic_detect_host();
    if (host.environment != SWAN_SIC_ENV_NATIVE ||
        (host.architecture != SWAN_SIC_ARCH_X86_64 &&
         host.architecture != SWAN_SIC_ARCH_ARM64))
        return false;

    const char *arch = host.architecture == SWAN_SIC_ARCH_X86_64 ? "x86_64" : "arm64";
    int n = snprintf(compiled_path, compiled_capacity, "%s.cwasm.%s", wasm_path, arch);
    if (n < 0 || (size_t)n >= compiled_capacity) return false;

    struct stat wasm_info, compiled_info;
    if (stat(wasm_path, &wasm_info) != 0) return false;
    if (stat(compiled_path, &compiled_info) == 0 &&
        compiled_info.st_size > 0 &&
        compiled_info.st_mtime >= wasm_info.st_mtime)
        return true;

    unlink(compiled_path);
    const char *engine = getenv("SWAN_WASM_ENGINE");
    if (!engine || !*engine) engine = "wasmtime";
    char *args[] = {(char *)engine, "compile", "--wasm", "memory64=y",
                    "-o", compiled_path, (char *)wasm_path, NULL};
    int result = swan_exec(args, true);
    if (result != 0) {
        unlink(compiled_path);
        return false;
    }
    if (stat(compiled_path, &compiled_info) != 0 ||
        compiled_info.st_size <= 0) {
        unlink(compiled_path);
        return false;
    }
    return true;
}

#endif
