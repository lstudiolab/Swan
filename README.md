# Swan

Swan is a compiled programming language with its own frontend, intermediate representation roadmap, optimizer roadmap, and machine-code backends. Swan source is compiled directly to assembly by Swan-owned code generators. It does not transpile to C and does not use LLVM or Clang.

## Current compiler architecture

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

Requirements: C++20 compiler and a system assembler/linker (normally provided by `cc`).

    sh build.sh
    ./build/swan examples/hello.swan
    ./build/swan build examples/hello.swan -o hello
    ./hello

## Backend status

| Target | Status |
|---|---|
| x86-64 System V | Initial Swan-owned assembly backend |
| AArch64 | Planned |
| ARMv7 / Thumb | Planned |
| RISC-V 64 | Planned |
| WebAssembly | Planned |

The x86-64 backend currently emits GNU assembler Intel syntax. Other architectures are rejected explicitly until their Swan-owned instruction selectors and ABI lowerers are implemented. Swan does not silently fall back to LLVM, Clang, or another compiler backend.

## Assembly and object output

    ./build/swan program.swan --target x86_64 --emit-asm -o program.s
    ./build/swan program.swan --target x86_64 --emit-obj -o program.o

`--target native` selects the host backend. The initial backend supports x86-64 System V. Object generation and executable linking use the system `CC` toolchain; set `CC` to select a compatible assembler/linker driver.

## Optimization

The native backend is an initial correctness-focused implementation. Swan-owned instruction selection, register allocation, peephole optimization, common-subexpression elimination, and target-specific scheduling are active compiler-development work. Do not treat the current `--optimize` flag as proof of a mature optimizer.

## Tests

    sh test.sh

CI builds the compiler without LLVM/Clang and checks native assembly emission, object generation, linking, execution, and explicit rejection of targets whose Swan backend is not yet implemented.
