# Swan

Swan is a compiled programming language with its own frontend, intermediate representation roadmap, optimizer roadmap, and machine-code backends. Swan source is compiled directly to assembly by Swan-owned code generators. It does not transpile to C and does not use LLVM or Clang.

## Current compiler architecture\n\nThe implementation is C11. `main.c` is the only compiled source file; the lexer, parser, AST, diagnostics, driver, and code generator are header-only C modules under `compiler/include/swan/`. No C++ compiler is required.

```text
.swan source
  -> Swan lexer
  -> Swan parser
  -> Swan semantic analysis
  -> checked Swan AST
  -> Swan-owned target code generator
  -> assembly
  -> system assembler and linker
  -> native executable
```

The assembler and linker are toolchain components used to turn emitted assembly into an object or executable. They do not perform Swan instruction selection, optimization, register allocation, or source compilation.

## Build

Requirements: C11 compiler and a system assembler/linker (normally provided by `cc`).

    sh build.sh
    ./build/swan examples/hello.swan
    ./build/swan build examples/hello.swan -o hello
    ./hello

## Backend status

| Target | Status |
|---|---|
| x86-64 System V | Initial Swan-owned assembly backend |
| AArch64 / ARM64 | Planned C backend |
| ARMv7 / Thumb | Planned |
| RISC-V 64 | Planned |
| WebAssembly | Planned |

The initial C backend emits GNU assembler Intel syntax for x86-64. Other architecture backends are not yet ported to C and are unavailable in this build. Swan does not silently fall back to LLVM, Clang, or another compiler backend.

## Assembly and object output

    ./build/swan program.swan --target x86_64 --emit-asm -o program.s
    ./build/swan program.swan --target x86_64 --emit-obj -o program.o
    ./build/swan program.swan --target aarch64 --emit-asm -o program-aarch64.s
    ./build/swan program.swan --target aarch64 --emit-obj -o program-aarch64.o

The initial C backend currently targets x86-64 System V on Linux. It supports the currently ported subset: integer expressions, local variables, function calls, if/else, while, and console logging. The system assembler/linker consumes Swan-emitted assembly; it does not compile Swan source.

## Optimization

The native backend is an initial correctness-focused implementation. A target-independent IR, register allocation, peephole optimization, common-subexpression elimination, and target-specific scheduling remain future compiler work.

## Tests

    sh test.sh

CI builds the C11 compiler, verifies standalone C headers and the single-translation-unit rule, and runs native x86-64 assembly execution tests.
