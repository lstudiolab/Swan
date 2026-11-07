# Compiler Architecture

Swan owns its frontend and machine-code generation. There is no C transpilation stage and no LLVM or Clang backend.

## Implementation model

The compiler implementation is C11 and header-only apart from \`main.c\`. Exactly one translation unit is compiled. Header modules own the AST, lexer, parser, diagnostics, semantic checks, driver, and code generator.

## Current pipeline

~~~text
.swan -> C lexer -> C parser -> semantic analysis -> Swan AST
      -> Swan-owned x86-64 code generator -> GNU assembly
      -> system assembler/linker -> executable
~~~

The host assembler and linker are used only after Swan has emitted assembly. They do not compile Swan source or choose machine instructions.

## Current implementation

- One compiled source file: \`main.c\`.
- Header-only C11 implementation in \`compiler/include/swan/\`.
- Direct AST-to-x86-64 System V assembly emission.
- GNU assembler Intel syntax.
- Semantic checks for duplicate functions, name resolution, function existence/arity, and immutable bindings.
- No C++ source or headers in the active implementation.

## Backend architecture target

Each target backend will own:
1. Target data layout and ABI lowering.
2. Instruction selection from Swan's typed low-level IR.
3. Register allocation and spill handling.
4. Calling convention and stack-frame layout.
5. Assembly emission and relocation/symbol conventions.
6. Target-specific optimization and instruction scheduling.

Planned C backends: AArch64, ARMv7/Thumb, RISC-V, and WebAssembly.

## Engineering rules

1. Swan source must never be transpiled into C.
2. Swan must not delegate code generation to LLVM or Clang.
3. An assembler/linker may consume Swan-emitted assembly; it is not a compiler backend.
4. Never advertise an architecture until its Swan-owned backend and ABI tests pass.
5. Every backend requires instruction-level tests, ABI tests, executable tests, and cross-target artifact validation.
