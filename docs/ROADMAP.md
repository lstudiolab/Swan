# Swan Compiler Roadmap

Swan's goal is to become a production-grade compiled language with predictable semantics, fast compilation, efficient native code, and a maintainable toolchain.

## Current baseline

The active pipeline is:

`.swan -> Lexer -> Parser -> semantic analysis -> typed AST -> LLVM IR -> LLVM target backend -> assembly/linker`

The C transpilation path and legacy standalone compiler have been removed. Swan now lowers the checked AST directly to LLVM IR. The driver accepts x86-64, AArch64, ARMv7, RISC-V 64, WebAssembly, and arbitrary LLVM target triples supported by the installed LLVM build. Cross-target assembly is supported independently of cross-linking; executable linking requires the target linker, runtime, and sysroot.

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

## Phase 3 — Typed IR correctness

- Verify every generated LLVM IR module before machine code generation.
- Add explicit IR dumps and stable regression fixtures.
- Complete lowering for all semantically accepted expressions, statements, aggregates, and calls.
- Add source-location metadata and debug information.
- Add differential tests against a trusted reference implementation for supported language semantics.

## Phase 4 — Native optimization framework

- Measure LLVM optimization behavior at each optimization level.
- Add Swan-owned optimizations only when benchmarks show a measurable benefit.
- Add compile-time profiling for frontend, lowering, optimization, and code emission.
- Track liveness, register allocation, spills, code size, and runtime through LLVM diagnostics and benchmark artifacts.
- Preserve source-level semantics with differential tests.

## Phase 5 — Native backend expansion

- Validate additional LLVM target triples and platform ABIs.
- Add cross-target object generation and sysroot-aware executable linking.
- Add target-specific runtime libraries and platform support.
- Add reproducible builds and architecture-specific CI runners.
- Add WebAssembly WASI and browser runtime integration.

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

- Fuzz lexer, parser, semantic analysis, IR lowering, and code generation.
- Add sanitizer, static-analysis, and cross-platform CI.
- Benchmark compile time, memory use, generated instruction quality, and runtime.
- Maintain backward compatibility and migration documentation.

## Engineering rules

1. Do not call placeholder features implemented.
2. Every language feature requires specification, implementation, diagnostics, and tests.
3. Keep frontend, IR, backend, and driver responsibilities separate.
4. Measure every optimization on reproducible benchmarks.
5. Never claim Swan is faster than another language without fair, repeatable measurements.
