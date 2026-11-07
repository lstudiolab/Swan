# Compiler Architecture

Swan is a native compiler. The frontend and semantic model are Swan-owned; LLVM is the machine backend.

## Repository layout

```text
main.cpp
compiler/include/swan/
├── Basic/       Source locations, tokens, diagnostics
├── Support/     File and version utilities
├── Lex/         Token scanners
├── AST/         Expression, declaration, statement, program nodes
├── Parse/       Grammar and parser
├── Sema/        Type, scope, declaration, statement analysis
├── Frontend/    Frontend orchestration
├── CodeGen/     Typed Swan AST to LLVM IR
├── Compiler/    Compilation orchestration
└── Driver/      Target selection and LLVM invocation
```

## Pipeline

```text
.swan -> Lexer -> Parser -> Semantic analysis
      -> typed AST -> LLVM IR -> LLVM target backend
      -> target assembly -> assembler/linker -> executable
```

Swan never emits C. The IR generator consumes the semantically checked AST directly. LLVM owns instruction selection, register allocation, machine optimization, and target assembly emission.

## Target model

The driver accepts common aliases (x86_64, aarch64, armv7, riscv64, wasm32) and full LLVM target triples. A target triple selects both the instruction set and the platform ABI. The installed LLVM backend determines which triples can be emitted.

Cross-target assembly does not require a sysroot. Linking a foreign executable requires the corresponding linker, runtime libraries, and sysroot.

## Engineering rules

1. Frontend owns Swan semantics and does not depend on code generation.
2. Code generation consumes a semantically checked AST.
3. LLVM IR is the backend interface; C is not an intermediate language.
4. Driver owns target selection and external tool invocation.
5. Every backend behavior requires a regression test.
6. Do not claim a target is supported until assembly generation and validation pass.
