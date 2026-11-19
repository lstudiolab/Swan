<p align="center"><img src="../assets/brew-logo.png" alt="Brew" width="180"></p>

# Svm Compiler Architecture

The normative compiler design is defined in [COMPILER_SYSTEM.md](compiler_system.md). This document summarizes SVM's role and current implementation boundary.

## Definition

SVM is Brew's **target-independent compiler middle-end**. It is not a virtual machine, runtime, bytecode format, interpreter, JIT, or target CPU.

Brew source is lowered directly from the checked AST into typed SVM IR. SVM verifies and optimizes that IR before the WASM64 backend emits the portable AOT module. SIC then coordinates host detection: browser hosts keep and execute WASM, while supported native hosts use Wasmtime's JIT to compile it into host machine code. No production executable dispatches over SVM instructions at runtime.

## Canonical pipeline

```text
Project source files
  -> per-file source manager, lexer and parser
  -> shared project/module graph
  -> name resolution and semantic/type analysis
  -> direct AST-to-SVM typed SSA lowering
  -> SVM verification
  -> target-independent SVM optimization
  -> final SVM verification and analysis rebuild
  -> target legalization
  -> instruction selection
  -> machine IR verification
  -> liveness and register allocation
  -> frame layout and ABI lowering
  -> machine-level optimization
  -> machine instruction encoding
  -> object emission
  -> native linking
  -> standalone AOT executable
```

WASM64 is the portable AOT artifact used by the canonical SIC path. A browser executes it through its native WebAssembly engine; a native host uses Wasmtime's runtime JIT to generate machine code for the detected CPU.

## Ownership boundaries

- **Frontend:** source identity, UTF-8 validation, lexing, parsing, AST, module graph, imports and diagnostics.
- **Semantic analysis:** declaration collection, name resolution, type checking, visibility, control-flow rules and ABI validation.
- **SVM:** target-independent typed SSA representation, verification, pass management and optimization.
- **Target backend:** target data layout, legalization, instruction selection, machine IR, register allocation, frame layout, ABI lowering and machine code.
- **Object writer:** sections, symbols, relocations and target object metadata.
- **Driver/linker:** deterministic input collection, command-line policy, native link orchestration and executable validation.

Each phase has explicit invariants and must fail with a diagnostic when its input violates them. An unsupported source construct, IR operation, target feature, ABI form, or relocation must never become a placeholder or silently run through an interpreter.

## Ir contract

SVM IR is typed SSA. The verifier is responsible for at least:

- valid function signatures and value types;
- unique and valid SSA definitions;
- dominance and use-before-definition rules;
- valid control-flow edges and terminators;
- PHI incoming values matching predecessor blocks;
- call target and argument signature agreement;
- valid memory operation pointer/value types;
- operation-specific operand and metadata constraints.

Verification is required before optimization, after transformations that can invalidate structural invariants, and before target lowering. A verifier failure is a compiler failure; the backend must not attempt to repair malformed IR silently.

## Optimization contract

The optimizer runs a bounded, change-driven fixed-point pipeline with a maximum of 350 pass executions. Its current pass sequence includes width-aware constant folding, CFG simplification, algebraic simplification, copy propagation, common-subexpression elimination, loop-invariant code motion, post-hoisting CSE, and dead-code elimination.

Every pass must declare required and preserved analyses, whether it mutates CFG or memory semantics, and its target dependencies. Integer transformations must preserve width, signedness and overflow semantics. Memory and call transformations must conservatively preserve observable side effects. PHI and CFG rewrites must preserve predecessor/value correspondence.

## Backend contract

A backend consumes only verified, optimized SVM IR. It owns target-specific legalization and machine lowering, and must document its supported operation subset, ABI, data layout, register model, relocation model and object format.

A backend is not considered production-complete merely because it emits assembly or an object. Completion requires target-specific compile, link, ABI, and execution tests (or WebAssembly module validation and execution tests).

Brew's long-term native backend is Brew-owned. LLVM/Clang code generation is not part of the intended production architecture. The existing optional LLVM bridge is transitional compatibility code; it must not be used as the native SVM implementation and must not be removed until replacement target coverage and migration tests exist.

## Aot and performance rules

1. No interpreter dispatch loop, VM, or bytecode execution path in production.
2. `brewc compile` emits a portable WASM64 module ahead of execution.
3. SIC detects browser/WebAssembly versus native execution environments before selecting the execution path.
4. Browser execution remains WebAssembly end to end; no Brew native JIT runs in the browser.
5. Native x86-64 and ARM64 execution uses Wasmtime's runtime JIT to generate code for the detected host CPU.
6. Temporary WASM is removed only after successful native runtime completion; explicit portable WASM outputs are retained.
7. Optimize target-independent IR before WASM emission and preserve all ABI, memory64, and host-runtime constraints.
8. Profile compiler phases separately from host JIT compilation and generated-program runtime.
9. Benchmark with correctness checks, repeated measurements, controlled settings and recorded hardware/toolchain versions. No unsupported cross-language speed claims.

## Current repository status

The repository has a single C11 production compiler, a header-oriented implementation, typed SSA IR, a verifier and optimizer, and Brew-owned emitters for several target families. It is not yet fully compliant with the long-term contract:

- the root `main.c` is a compatibility include of `compiler/tools/brewc/main.c`;
- source files are lexed and parsed independently and retain stable source paths; native x86-64 project builds emit one assembly/object translation unit per source file, while cross-target backends still consume a project-wide IR module;
- backend and object/link orchestration remains target-specific in the driver;
- native assembly/object production still uses external toolchain components;
- the optional LLVM bridge remains available for explicit target triples;
- target source coverage and ABI completeness are unequal.

See the migration sequence in [COMPILER_SYSTEM.md](compiler_system.md). Do not describe a transitional feature as complete until its end-to-end tests establish that claim.
