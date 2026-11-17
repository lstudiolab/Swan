# SVM Compiler Architecture

The normative compiler design is defined in [COMPILER_SYSTEM.md](COMPILER_SYSTEM.md). This document summarizes SVM's role and current implementation boundary.

## Definition

SVM is Swan's **target-independent compiler middle-end**, not a virtual machine, runtime, bytecode format, interpreter, JIT, or target CPU.

Swan source is lowered directly from the checked AST into typed SVM IR. SVM verifies and optimizes that IR before handing it to Swan-owned target backends. No production executable dispatches over SVM instructions at runtime.

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

WebAssembly is a separate AOT output target. It is not a native executable and requires a compatible WebAssembly host.

## Ownership boundaries

- **Frontend:** source identity, UTF-8 validation, lexing, parsing, AST, module graph, imports and diagnostics.
- **Semantic analysis:** declaration collection, name resolution, type checking, visibility, control-flow rules and ABI validation.
- **SVM:** target-independent typed SSA representation, verification, pass management and optimization.
- **Target backend:** target data layout, legalization, instruction selection, machine IR, register allocation, frame layout, ABI lowering and machine code.
- **Object writer:** sections, symbols, relocations and target object metadata.
- **Driver/linker:** deterministic input collection, command-line policy, native link orchestration and executable validation.

Each phase has explicit invariants and must fail with a diagnostic when its input violates them. An unsupported source construct, IR operation, target feature, ABI form, or relocation must never become a placeholder or silently run through an interpreter.

## IR contract

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

Swan's long-term native backend is Swan-owned. LLVM/Clang code generation is not part of the intended production architecture. The existing optional LLVM bridge is transitional compatibility code; it must not be used as the native SVM implementation and must not be removed until replacement target coverage and migration tests exist.

## AOT and performance rules

1. No interpreter dispatch loop, VM, or bytecode execution path in production.
2. `run` builds a host-native executable and launches it as a separate process.
3. Cross-compilation emits destination-target artifacts; the host does not execute foreign machine code.
4. Optimize target-independent IR before legalization; apply machine-level optimization after instruction selection.
5. Respect target ABI, register classes, stack alignment, frame rules, and relocations.
6. Profile compiler phases separately from generated-program runtime.
7. Benchmark with correctness checks, repeated measurements, controlled settings and recorded hardware/toolchain versions. No unsupported cross-language speed claims.

## Current repository status

The repository has a single C11 production compiler, a header-oriented implementation, typed SSA IR, a verifier and optimizer, and Swan-owned emitters for several target families. It is not yet fully compliant with the long-term contract:

- the root `main.c` is a compatibility include of `compiler/tools/swanc/main.c`;
- source files are lexed and parsed independently and retain stable source paths; native x86-64 project builds emit one assembly/object translation unit per source file, while cross-target backends still consume a project-wide IR module;
- backend and object/link orchestration remains target-specific in the driver;
- native assembly/object production still uses external toolchain components;
- the optional LLVM bridge remains available for explicit target triples;
- target source coverage and ABI completeness are unequal.

See the migration sequence in [COMPILER_SYSTEM.md](COMPILER_SYSTEM.md). Do not describe a transitional feature as complete until its end-to-end tests establish that claim.
