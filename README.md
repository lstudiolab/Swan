# Swan

Swan is a compiled programming language with its own lexer, parser, semantic analysis, and native LLVM IR lowering. It does **not** transpile Swan source into C.

## Compilation architecture

```text
.swan source
  -> Lexer
  -> Parser
  -> Typed AST + semantic analysis
  -> Swan LLVM IR lowering
  -> LLVM target backend
  -> target assembly / object / executable
```

The same checked Swan program is lowered once to LLVM IR. LLVM performs target-specific instruction selection, register allocation, machine-level optimization, and assembly emission. There is no C source generation stage and no C compiler subprocess.

## Build

Requirements: C++20 compiler and Clang/LLVM.

    sh build.sh
    ./build/swan examples/hello.swan
    ./build/swan build benchmarks/binary_search.swan -o binary-search
    ./binary-search

## Target architectures

Swan exposes common target aliases:

| Alias | Target |
|---|---|
| `x86_64` | 64-bit x86 |
| `aarch64` | 64-bit ARM |
| `armv7` | 32-bit ARM |
| `riscv64` | 64-bit RISC-V |
| `wasm32` | WebAssembly 32-bit |

You can also pass a full LLVM target triple to select an operating-system ABI and device target, for example:

- `aarch64-apple-darwin`
- `arm64-apple-ios`
- `aarch64-linux-android24`
- `armv7a-linux-androideabi24`
- `x86_64-pc-windows-msvc`

Emit assembly:

    ./build/swan program.swan --target aarch64 --emit-asm -o program.s
    ./build/swan program.swan --target wasm32 --emit-asm -o program.s
./build/swan program.swan --target aarch64 --emit-obj -o program.o
./build/swan program.swan --target aarch64 --cpu cortex-a72 --features +neon --emit-asm -o program.s

Build and run on the host:

    ./build/swan build program.swan -o program
    ./program

Cross-target assembly and object generation do not require a target sysroot. Linking a runnable executable for a foreign operating system requires that platform's linker, runtime libraries, and sysroot. Use `--cpu` and `--features` to select a concrete CPU and ISA extensions. Actual target availability depends on the installed LLVM build.

## Optimization and compilation speed

    ./build/swan program.swan --optimize
    ./build/swan program.swan --fast-compile

The optimized path uses LLVM's high optimization level. Fast compile mode prioritizes lower compilation latency.

## Binary-search workload

`benchmarks/binary_search.swan` contains the 20,000,000-element / 20,000,000-query workload.

    ./build/swan build benchmarks/binary_search.swan -o binary-search
    ./binary-search

Expected checksum:

    199999990000000

## Tests

    sh test.sh

CI validates the native backend, language regressions, and assembly generation for x86-64, AArch64, ARMv7, RISC-V, WebAssembly, Apple, Android, and Windows targets.
