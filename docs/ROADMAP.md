# Swan Compiler Roadmap

Swan's goal is to become a production-grade programming language and compiler. The goal is capability, correctness, performance, and maintainability—not source-code size for its own sake.

The compiler keeps the project's requested source layout:
- One C++ translation unit: `main.cpp`
- Stage implementations in focused headers under `compiler/include/swan/`
- No CMake requirement

## Current baseline

The current compiler is a C++20 source-to-source compiler:
`.swan -> Lexer -> Parser -> AST -> Sema -> C generator -> .c`

The supported language is still a subset. A recognized keyword is not considered implemented until parsing, semantic analysis, code generation, diagnostics, and tests all support its defined behavior.

## Phase 1 — Language specification and frontend completeness

- Publish a formal Swan grammar and language specification.
- Define exact semantics for every registered keyword.
- Complete declarations, expressions, functions, structs, enums, classes, imports, exports, and access control.
- Add type inference, generic types, overload resolution, and compile-time validation.
- Improve diagnostics with source spans, notes, and actionable suggestions.
- Add parser recovery and comprehensive positive/negative conformance tests.

**Exit condition:** every documented frontend feature has specification examples and passing tests; unsupported syntax is rejected clearly.

## Phase 2 — Swan memory and execution semantics

- Specify and implement `hold`, `store`, and `clone`.
- Specify ownership, borrowing, lifetimes, mutation, and destruction rules.
- Implement `operate` recovery semantics with deterministic failure behavior.
- Add static lifetime and escape analysis.
- Prove that generated operations preserve the specified runtime behavior.

**Exit condition:** memory behavior is defined, statically checked where possible, and covered by stress and safety tests.

## Phase 3 — SIL intermediate representation

Only begin after the language semantics and frontend stabilize.

- Design typed SIL with explicit control flow and data flow.
- Add a verifier that checks every SIL function and instruction.
- Lower the AST into SIL without losing source-level meaning.
- Add readable SIL dumps for debugging and regression tests.

**Exit condition:** every supported Swan program lowers to valid, verifiable SIL.

## Phase 4 — Optimization framework

- Add demand analysis.
- Add constant folding and propagation.
- Add dead-code and unreachable-block elimination.
- Add common-subexpression elimination.
- Add inlining and specialization where profitable.
- Add escape analysis and allocation elimination.
- Add optimization correctness tests and benchmark comparisons.

Every optimization must preserve program semantics and be independently testable.

## Phase 5 — Code generation backends

- Keep the portable C backend.
- Define a target-independent backend interface.
- Add a native backend when the SIL and target model are stable.
- Support target triples, ABI rules, debug information, and reproducible output.
- Test generated programs against the same language conformance suite.

## Phase 6 — Runtime and standard library

- Strings, arrays, collections, iterators, and algorithms.
- File and console I/O.
- Error handling and process APIs.
- Date/time, math, and text utilities.
- Networking and concurrency primitives.
- Platform abstraction and cross-platform tests.

Runtime behavior must follow Swan's memory and concurrency rules.

## Phase 7 — Developer ecosystem

- Package manager and dependency resolution.
- Formatter and linter.
- Language server protocol implementation.
- Debug information and debugger integration.
- Test runner, documentation generator, and build tooling.
- Stable compiler interfaces and versioned language releases.

## Phase 8 — Production hardening

- Fuzzing for lexer, parser, semantic analysis, SIL, and code generation.
- Differential and property-based tests.
- Sanitizer and static-analysis CI.
- Compile-time, memory, and runtime benchmarks.
- Cross-platform CI and release artifacts.
- Backward-compatibility and migration policy.

## Engineering rules

1. Do not add placeholder features and call them implemented.
2. Do not start SIL before the frontend and language semantics stabilize.
3. Every feature needs specification, implementation, diagnostics, and tests.
4. Keep stage responsibilities separated even though the implementation uses one `main.cpp` translation unit.
5. Measure compile time, generated-code quality, memory use, and runtime performance.
6. Compare Swan with Goose using reproducible benchmarks and documented feature tests, not line counts.
