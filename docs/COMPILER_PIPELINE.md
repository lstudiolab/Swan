# Swan Compiler Pipeline

## Overview

Swan's production compiler combines a C11 host frontend/orchestrator, a small Swan-authored SIL subsystem, a general typed-IR pipeline, LLVM/Clang optimized x86-64 machine lowering, Swan-owned cross-target emitters, and an x86-64 AST compatibility backend.

## Build-time bootstrap

The build process is separate from compiling a user program:

```text
build.sh
  -> compile main.c as temporary C11 bootstrap compiler
  -> concatenate compiler/sil/*.swan stages
  -> bootstrap compiler emits SIL runtime assembly
  -> system assembler creates SIL runtime object
  -> link main.c + SIL runtime object
  -> build/swan
```

The SIL object is generated from Swan source and linked into the final compiler. The C11 host still owns CLI orchestration, lexing, parsing, AST, semantic analysis, general IR, optimization, and backends.

## User-program compilation

```text
source file
  -> read and validate UTF-8
  -> lexer
  -> indentation-aware parser / AST
  -> C-header declaration import
  -> Swan standard-library module loading
  -> semantic analysis
  -> function reachability
  -> source lowering
       eligible scalar functions: AST -> Swan SIL -> typed IR
       other IR-supported functions: AST -> typed IR
       x86-only compatibility features: AST -> x86-64 assembly
  -> SVM shared compile core: typed-IR verification and optimization
  -> selected target code generation
  -> assembly/object/module emission
  -> assembler/linker (when applicable)
```

The SIL route is optional per function. The bridge starts with a complete general typed-IR module and substitutes SIL-lowered functions only when the narrow SIL subset accepts them. Unsupported SIL constructs do not force a failure when the general IR path supports them.

The x86-64 typed-IR path uses LLVM/Clang at -O3 for machine instruction selection, register allocation, scheduling, and assembly emission. The x86-64 compatibility path remains necessary for supported constructs not yet lowered into IR, including current array/output and lazy-import behavior. Cross-target backends use typed IR and do not inherit these AST-only features.

## Typed IR optimization

The typed-IR pass manager currently includes:

1. Width-aware constant folding.
2. Constant-select folding.
3. Constant-branch folding and CFG simplification.
4. Unreachable-block pruning.
5. Algebraic simplification.
6. Trivial-PHI copy propagation.
7. Dominance-aware common-subexpression elimination.
8. Conservative loop-invariant code motion.
9. A post-hoisting CSE pass.
10. Dead-code elimination using value-worklist liveness propagation.

Potentially trapping operations and memory effects must be preserved. IR is always verified before optimization and after final cleanup. Debug builds also verify every pass boundary; optimized release builds skip redundant intermediate whole-module verifier walks and retain the final verification gate. SSA value types are indexed by value ID for constant-time type queries. Optimizations are semantics-preserving and target-independent.

## Swan-authored SIL subsystem

The SIL stages are in `compiler/sil/` and are compiled during `build.sh`. The current instruction stream uses five 32-bit words per instruction: encoded opcode/type, destination, left operand, right operand, and immediate/parameter index.

The current builder/pass subset includes parameter records, constants, add/subtract/multiply, equality, signed-less-than, copy, and return. It is deliberately bounded to 256 value IDs and 256 instructions; packed immediates are restricted to non-negative values up to 65535.

The bridge accepts only straight-line functions with one return expression and scalar parameter/literal/arithmetic/comparison forms. It is not the general IR and does not yet implement CFG, branches, variables, memory, general calls, loops, or imports. General typed-IR lowering remains the fallback for other supported functions.

## Backend selection

- `--target x86_64`: native System V Linux code generation; may use typed IR or AST compatibility lowering.
- `--target aarch64`, `armv7`, `riscv64`: Swan-owned limited scalar IR backends that emit target objects/assembly.
- `--target wasm32`, `wasm64`: Swan-owned limited WebAssembly module emitters.
- Native x86-64 typed IR: emitted as LLVM IR and lowered by Clang -O3 to native assembly.\n- `--target-triple <triple>`: explicit LLVM portability bridge. Requires an installed Clang/LLVM target backend and is restricted to Swan typed IR.

SVM is the shared compiler engine, not a target. The legacy IR interpreter and `.swvm` target have been removed. The CLI's target list identifies available generators, not full source-language parity. Each backend has its own supported types, operations, argument limits, and ABI restrictions. Normal execution is always ahead-of-time machine-code execution; there is no SVM instruction interpreter. See [ARCHITECTURE.md](ARCHITECTURE.md) and the root README for the current summary.

## Failure behavior

Unknown semantic or IR nodes, invalid IR, unsupported backend instructions, and unsupported target/language combinations must produce diagnostics. The compiler must not silently generate empty code or silently switch to C/LLVM code generation.
