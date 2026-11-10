# swanc — Swan's self-hosted compiler

`swanc` is the planned production compiler for Swan, implemented in Swan itself. It is a compiler, not an interpreter. It lowers Swan source into the target-independent SVM representation, verifies and optimizes that representation, then emits native machine code or a WebAssembly module for the selected target.

This document defines the bootstrap contract and implementation boundaries. It does not claim that the compiler is already self-hosted.

## Why self-hosting

Self-hosting is not required for fast generated programs. Runtime performance comes from the SVM optimizer, target-specific instruction selection, register allocation, and code generation. Self-hosting is valuable because it makes Swan's compiler implementation a real Swan program, exercises the language against a substantial codebase, and lets the language maintain and improve its own compiler.

## Compiler boundary

The intended pipeline is:

```text
source files
  -> source manager
  -> UTF-8 lexer
  -> indentation-aware parser
  -> AST
  -> name resolution and type checking
  -> typed SVM SSA IR
  -> IR verifier
  -> target-independent optimization
  -> target lowering and register allocation
  -> machine-code / object / WASM emission
  -> optional system linker
```

SVM remains the shared compilation engine and intermediate representation. It is not a target, virtual machine, bytecode interpreter, or runtime execution loop.

The initial supported targets remain x86-64, AArch64, ARMv7, RISC-V64, WASM32, and WASM64. Target selection must happen after target-independent verification and optimization. A target backend must reject unsupported IR operations instead of silently emitting placeholder code.

## Source layout

The self-hosted compiler will be split into small Swan modules:

- `swanc/driver.swan` — command-line options, compilation orchestration, diagnostics.
- `swanc/source.swan` — source buffers, file loading boundary, source spans.
- `swanc/lexer.swan` — UTF-8-aware tokenization and indentation tokens.
- `swanc/parser.swan` — precedence parser, declarations, statements, and type syntax.
- `swanc/ast.swan` — AST node representation and construction.
- `swanc/sema.swan` — symbol resolution, type checking, ownership checks, reachability.
- `swanc/ir.swan` — typed SSA values, blocks, instructions, and terminators.
- `swanc/irgen.swan` — AST-to-SVM lowering.
- `swanc/verify.swan` — structural, typing, dominance, and control-flow verification.
- `swanc/optimize.swan` — target-independent optimization pipeline.
- `swanc/targets/*.swan` — target lowering and emitters.
- `swanc/link.swan` — system toolchain invocation and artifact handling.

The implementation may use compact integer-indexed arenas and tables during bootstrap. It must not depend on an interpreter to execute compiler passes.

## Bootstrap stages

### Stage 0 — existing compiler

The current compiler is the trusted bootstrap compiler. It compiles the first Swan implementation of `swanc`. Stage 0 is a temporary build tool, not the final architecture.

### Stage 1 — Swan-authored compiler

Stage 0 compiles the `swanc` source modules into a native `swanc` executable. The compiler logic—lexing, parsing, semantic analysis, IR construction, verification, optimization, and code generation—must progressively move into Swan source.

A narrow host interface may initially provide operating-system operations such as reading files, writing artifacts, and launching the system linker. These calls are platform services, not a second compiler implementation. The host interface must shrink as Swan gains the required standard-library and platform capabilities.

### Stage 2 — self-compilation

Stage 1 compiles the same `swanc` source tree and produces Stage 2. Stage 2 must successfully compile the Swan test suite and representative programs for every implemented target.

### Stage 3 — reproducibility

Stage 2 compiles the same source again. Compare normalized compiler outputs and run the full test suite. Exact binary reproducibility is a later goal if timestamps, paths, or linker metadata make byte-for-byte comparison inappropriate.

Keep Stage 0 available until Stage 2 is reliable. Do not delete the bootstrap compiler prematurely.

## Required language capabilities

Before the compiler can be fully self-hosted, Swan must provide reliable support for the constructs used by its own implementation:

- fixed-width integer types and checked conversions;
- structs or an equally safe representation for compiler records;
- arrays, slices, and arena-backed allocation;
- byte strings and UTF-8 operations;
- file and command-line APIs through a small platform layer;
- deterministic diagnostics and source spans;
- function pointers or an explicit alternative for pass and target dispatch;
- complete type checking and ownership validation for the subset used by the compiler.

Do not add language features speculatively. Add each capability when a real `swanc` module requires it, with tests and documented semantics.

## Implementation order

1. Port the source manager and lexer; test token kinds, spans, UTF-8, comments, and indentation.
2. Port the parser and AST; add parse-tree golden tests.
3. Port name resolution and type checking; require errors for every unsupported construct.
4. Port typed SVM IR construction and the verifier.
5. Port optimization passes and compare optimized IR against the existing compiler.
6. Port target lowering and code generation, starting with x86-64, then AArch64, ARMv7, RISC-V64, WASM32, and WASM64.
7. Add Stage 0 → Stage 1 → Stage 2 bootstrap CI and compile the compiler with itself.
8. Retain cross-compiler tests and differential execution tests to catch semantic drift.

## Bootstrap invariants

- No interpreter is introduced as a shortcut.
- The compiler emits native code or WebAssembly; SVM IR is never executed by an interpreter.
- Every IR module is verified before and after optimization.
- Unsupported syntax, types, ownership cases, or target operations are hard errors.
- The compiler source must not use language features that the current Stage 0 compiler accepts only by silently ignoring them.
- Stage 1 and Stage 2 must pass the same conformance suite.
- Keep the existing compiler as Stage 0 until self-hosting and cross-target tests are green.

## Current status

This is the architecture and bootstrap plan. The existing repository has Swan-authored SIL stages, but the complete lexer/parser/semantic/IR/backend implementation is not yet written in Swan. Therefore Swan is not yet fully self-hosted.