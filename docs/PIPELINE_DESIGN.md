# Swan Compiler Pipeline Design

This document records the pre-SIL compiler architecture and the boundary between implemented behavior and planned work. SIL has not been started.

## Implemented pipeline

```text
.swan
  -> lexer.h
  -> parser.h + ast.h
  -> import resolution
  -> sema.h
  -> irgen.h
  -> optimization.h
  -> codegen_<target>.h
  -> emitter.h
  -> linker.h / target assembler
  -> object, executable, WebAssembly module, or .swvm
```

The x86-64 compatibility path can still lower frontend constructs not yet represented in IR (notably arrays, logging and runtime-lazy imports) through its established AST backend. The IR path is preferred for the supported scalar subset and is used by x86-64 plus the cross-target backends.

## Stage contracts

| Stage | Input | Output | Contract |
|---|---|---|---|
| Lexer | UTF-8 source bytes | Token stream | Preserve source locations; report lexical errors. |
| Parser | Tokens | AST | Parse syntax only; allocate AST nodes in the compilation arena. |
| AST | — | AST data model | Own node/type definitions and source spans. |
| Semantic analysis | AST + imported module interfaces | Validated AST | Resolve names, scopes, types and control-flow constraints. |
| IR generation | Validated AST | Typed target-independent SSA IR | Explicit values, control-flow blocks, calls, memory operations and terminators. |
| IR optimization | IR | Verified optimized IR | Constant folding, constant select folding, CFG simplification, algebraic simplification, block-local CSE and iterative DCE. |
| Code generation | Optimized IR + target | Assembly or binary module | Target instruction selection, ABI argument/return handling, frame layout, branch/PHI lowering. |
| Emitter | Assembly text | File output | Write generated assembly without changing its meaning. |
| Linker | Assembly/object + toolchain | Object or executable | Invoke the selected assembler/linker; no compiler-backend fallback. |
| SVM | Typed IR / .swvm | Portable execution | Interpret verified IR with bounded fuel and call depth. |

## Required ordering rules

1. ast.h defines AST structures; parser.h constructs them.
2. Import/module loading precedes whole-program semantic analysis.
3. Every IR optimization pass verifies the IR after transformation.
4. irgen.h does not select target instructions.
5. optimization.h does not perform target-specific machine peepholes.
6. codegen headers do not own assembly file serialization.
7. emitter.h and linker.h remain separate pipeline stages.
8. No stage may silently fall back to C transpilation or LLVM/Clang code generation. Clang may be invoked only as a target assembler driver on Swan-emitted assembly.

## Implemented IR and SVM contracts

- IR types: void, i1, i8, i16, i32, i64, f32, f64 and ptr.
- Operations: integer arithmetic/division/remainder, bitwise operations, shifts, comparisons, select, PHI, alloca/load/store and direct calls.
- Verifier: SSA definition uniqueness and bounds, use-before-definition, dominance, branch targets, terminators, PHI predecessor coverage/types, call signatures and operand typing.
- IR generation: integer/boolean literals and expressions, direct calls, immutable and mutable locals, assignment, if/else, while, break/continue and short-circuit AND/OR.
- SVM: register-based execution, recursive calls, local memory, PHI/CFG execution, typed integer-width behavior, traps, fuel and call-depth limits.
- .swvm: versioned little-endian artifact with bounded reader/writer, malformed/truncated/trailing-data checks and explicit string ownership.
- IR optimization: constant folding with declared integer widths, constant select folding, constant-branch folding and unreachable-block removal, algebraic identities, block-local common-subexpression elimination and iterative dead-code elimination. Potentially trapping division/remainder and memory loads are not discarded as dead pure instructions.

## Target status

| Target | Output | Implemented subset |
|---|---|---|
| x86_64 | Linux ELF executable / assembly | IR scalar subset plus legacy AST compatibility path for arrays, logging and lazy imports |
| aarch64 | Linux ELF object / assembly | i1/i64 scalar IR, calls, memory locals, CFG and PHI |
| armv7 | ELF object / assembly | i32/i1 scalar IR, calls within register ABI limit, memory locals and CFG; division requires runtime helpers and is rejected |
| riscv64 | Linux ELF object / assembly | Integer scalar IR, calls, memory locals, CFG and PHI |
| wasm32 | WebAssembly module | i32/i64 scalar IR, calls, recursion, CFG/PHI, linear memory and per-invocation stack frames |
| wasm64 | WebAssembly memory64 module | Scalar emission path; memory64 execution depends on runtime support |
| svm | .swvm portable module | Verified IR interpreter |

Target availability does not mean every Swan source construct is supported on every target. The current IR generator does not yet lower array/slice operations, strings/logging, floating-point arithmetic, closures, aggregates or runtime-lazy imports. Cross-target object emission is not the same as running the object on the current host. Windows x64, Apple Darwin ABIs, ARM Thumb and additional RISC-V profiles are not implemented.

## Pre-SIL exit gates

Before SIL begins:
- Keep the full existing .swan compiler/stdlib suite green.
- Keep typed IR verifier tests and IR-generation tests green.
- Keep optimization pass tests green, including CFG/PHI cases and typed integer overflow.
- Keep SVM artifact round-trip, malformed-input and runtime tests green.
- Keep native x86-64 IR execution and legacy compatibility tests green.
- Keep WebAssembly runtime validation/execution tests green.
- Keep AArch64, ARMv7 and RISC-V64 object-format tests green.
- Preserve explicit diagnostics for unsupported target/language combinations.
- Do not advertise full target parity until ABI execution tests cover the advertised subset.

SIL remains the next architecture phase only after these gates and feature boundaries are stable.
