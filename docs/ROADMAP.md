# Swan Compiler Roadmap

Swan's goal is to become a production-grade compiled language with predictable semantics, fast compilation, efficient native code, and a maintainable toolchain.

## Current baseline

The active pipeline is:

`.swan -> Lexer -> Parser -> AST -> Semantic analysis -> x86-64 GNU assembly -> assembler/linker`

The direct assembly backend currently targets x86-64 Linux System V ELF. It supports the present scalar language subset, basic control flow, function calls, strings, and console output. Other targets, aggregate ABI lowering, register allocation, and an optimizing IR are not yet complete.

## Phase 1 — Language specification and frontend completeness

- Publish a formal Swan grammar and language specification.
- Define exact semantics for every registered keyword.
- Complete declarations, expressions, functions, structs, enums, classes, imports, exports, and access control.
- Add type inference, generic types, overload resolution, and compile-time validation.
- Improve diagnostics with source spans, notes, and actionable suggestions.

## Phase 2 — Swan memory and execution semantics

- Specify and implement hold, store, and clone.
- Specify ownership, borrowing, lifetimes, mutation, and destruction.
- Implement operate recovery semantics with deterministic failure behavior.
- Add static lifetime and escape analysis.

## Phase 3 — Typed low-level IR

- Introduce a typed, verified intermediate representation between semantic analysis and machine code.
- Represent control flow as basic blocks with explicit terminators.
- Make integer, floating-point, pointer, and aggregate operations explicit.
- Add IR dumps and verifier tests.

## Phase 4 — Native optimization framework

- Add constant folding, propagation, and dead-code elimination.
- Add liveness analysis, register allocation, and spill minimization.
- Add common-subexpression elimination and loop-invariant code motion.
- Add strength reduction, loop transformations, and instruction selection.
- Add inlining and specialization only when benchmarks show a benefit.
- Preserve source-level semantics with differential tests.

## Phase 5 — Native backend expansion

- Add AArch64 and additional platform targets.
- Implement aggregate and structure layout/ABI lowering.
- Add debug information, target triples, and reproducible object generation.
- Improve calling conventions and external ABI interoperability.

## Phase 6 — Runtime and standard library

- Strings, arrays, collections, iterators, and algorithms.
- File and console I/O, errors, process APIs, and math.
- Networking and concurrency primitives.
- Platform abstraction and cross-platform tests.

## Phase 7 — Developer ecosystem

- Package manager and dependency resolution.
- Formatter and linter.
- Language server protocol implementation.
- Debugger integration and test runner.
- Stable compiler interfaces and versioned releases.

## Phase 8 — Production hardening

- Fuzz lexer, parser, semantic analysis, IR, and code generation.
- Add sanitizer, static-analysis, and cross-platform CI.
- Benchmark compile time, memory use, generated instruction quality, and runtime.
- Maintain backward compatibility and migration documentation.

## Engineering rules

1. Do not call placeholder features implemented.
2. Every language feature requires specification, implementation, diagnostics, and tests.
3. Keep frontend, IR, backend, and driver responsibilities separate.
4. Measure every optimization on reproducible workloads.
5. Never claim Swan is faster than C without fair, repeatable measurements.