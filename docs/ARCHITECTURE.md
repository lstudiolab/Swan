# Compiler Architecture

Swan owns its compilation pipeline and machine-code backends. There is no C transpilation stage, LLVM IR backend, or Clang code-generation dependency.

## Current pipeline

```text
.swan -> Lexer -> Parser -> Semantic analysis -> checked AST
      -> Swan-owned x86-64 code generator -> GNU assembly
      -> system assembler/linker -> object / executable
```

The host assembler and linker are only used after Swan has emitted assembly. They do not compile Swan source or choose machine instructions.

## Current implementation

- Frontend: Swan lexer, parser, and semantic analysis.
- Code generation: direct checked-AST to x86-64 System V and AArch64 AAPCS64 ELF assembly.
- Assembly syntax: GNU assembler Intel syntax for x86-64; GNU AArch64 syntax for ARM64.
- Object production: host system `CC` for x86-64 and GNU AArch64 assembler for ARM64.
- Unsupported targets fail with a diagnostic. There is no LLVM or external compiler fallback.

## Backend architecture target

Each target backend will own:
1. Target data layout and ABI lowering.
2. Instruction selection from Swan's typed low-level IR.
3. Register allocation and spill handling.
4. Calling convention and stack-frame layout.
5. Assembly emission and relocation/symbol conventions.
6. Target-specific optimization and instruction scheduling.

Planned backends: x86-64, AArch64, ARMv7/Thumb, RISC-V 64, and WebAssembly.

## Engineering rules

1. Swan source must never be transpiled into C.
2. Swan must not delegate code generation to LLVM or Clang.
3. An assembler/linker may consume Swan-emitted assembly; it is not a compiler backend.
4. Never advertise an architecture until its Swan-owned backend and ABI tests pass.
5. Every backend requires instruction-level tests, ABI tests, executable tests, and cross-target artifact validation.
