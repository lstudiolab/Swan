# Swan language completion plan

**Status:** implementation program, not a claim that the listed capabilities already exist.
**Baseline:** `main` after compiler-managed-memory foundation PR #27.
**Rule:** a feature is complete only when it is represented in the frontend, checked semantically, lowered through SIL and SVM IR, emitted by every target that claims support, and covered by positive and negative tests.

## Completion contract

Swan is intended to be a general-purpose, ahead-of-time compiled language for systems software, command-line tools, services, web/WASM, scientific and numerical workloads, AI infrastructure, games/graphics, and embedded software. This is a target, not the current capability claim.

The compiler must preserve one-source-file-to-one-compilation-unit behavior. Multi-file programs are linked from independently compiled units; the compiler must not silently combine an arbitrary number of source files into one assembly output.

## Current architecture gaps

| Area | Current implementation | Required completion |
|---|---|---|
| Lexer | UTF-8 validation, indentation tokens, 33 reserved words; identifier classification treats any high byte as an identifier character | Unicode XID_Start/XID_Continue, normalization policy, precise malformed-input locations, complete literal/escape rules, generated keyword registry |
| Parser | Indentation-based functions, declarations, expressions, nested if/else, while/loop, break/continue | Full grammar for declarations, types, aggregates, generics, patterns, errors, concurrency, modules, attributes, and nested expressions/statements |
| AST | Scalar expressions and a small statement set | Explicit nodes for types, aggregate declarations/values, patterns, closures, async operations, defer, exceptions, and compile-time constructs |
| Semantic analysis | Function/local symbol tables, scalar types, limited control-flow checks, legacy array ownership rules | Type inference/checking, overload and generic resolution, trait/protocol conformance, visibility, definite initialization, exhaustiveness, effect checking, borrow/move/escape analysis |
| SIL | Swift-inspired CFG, ownership categories, verifier and small transforms; current SILGen is a validation-stage representation | Canonical typed high-level IR with real local/parameter bindings, typed operands, aggregate operations, ownership dataflow, cleanup insertion, and SIL-to-SVM lowering |
| SVM IR | Typed scalar SSA, calls, memory primitives, PHI/CFG and scalar optimizations | Aggregate, pointer/provenance, vector, atomic, exception, coroutine, debug and target-independent ABI representations |
| Memory | Compiler-generated cleanup exists for a constrained native array path; memory foundation is documented in PR #27 | Language-wide stack-first allocation, escape analysis, move/borrow checking, deterministic destruction on all exits, heap promotion, interprocedural summaries, and equivalent behavior across targets |
| Optimizer | A shared maximum budget of 350 executions across the current SIL and SVM middle-end | 350 registered pass slots distributed across lexing/frontend, semantic analysis, SIL, SVM IR, target lowering and machine/finalization stages; each pass must have an invariant, verifier boundary, profile counter and tests. A budget of 350 executions is not the same as 350 distinct passes |
| Backends | x86-64 is the only target accepted by the main `swan_codegen` dispatch; other SVM emitters exist for AArch64, ARMv7, RISC-V64, WASM32/64 and LLVM | Explicit per-feature target capability declarations, ABI-complete lowering, runtime support, object emission/linking, and parity tests for each supported target |
| Runtime and standard library | Small Foundation/std/math/collections/JSON and selected OS/network primitives | Stable core, memory, text/Unicode, collections, filesystem, process, networking, concurrency, serialization, crypto, math, SIMD, graphics/audio, game, embedded and WASI interfaces |
| Tooling | CLI, tests, partial VS Code support | Language server, formatter, package/build system, debugger metadata, diagnostics, documentation generator and reproducible cross-compilation |

## Reserved-word disposition

A token being recognized by the lexer does not make its language feature implemented.

| Word | Current status | Completion requirement |
|---|---|---|
| `func` | Implemented | Add generic/async/effect/attribute forms |
| `let`, `var`, `const` | Implemented for local scalar declarations; `const` currently behaves as immutable local | Global/module constants, compile-time evaluation and typed initialization |
| `if`, `else`, `while`, `loop`, `break`, `continue` | Implemented for nested control flow | Flow-sensitive initialization, labeled control flow and complete cleanup edges |
| `return`, `pass` | Implemented | Return-value checking and cleanup on every exit |
| `true`, `false` | Implemented | None |
| `import`, `export` | Implemented in limited module/function forms | Visibility, re-exports, package resolution and stable ABI |
| `self` | Token accepted as a name | Receiver typing, methods and associated items |
| `as` | Reserved by lexer; no cast/alias grammar | Checked casts, coercions and type aliases with explicit syntax |
| `null` | Reserved and explicitly rejected | Typed optional/reference-nullability model and safe operations |
| `from`, `upto` | Reserved; no range-loop grammar | Range expressions, iterator protocol and checked loop lowering |
| `type`, `struct`, `enum` | Reserved; declaration grammar absent | Nominal types, layout, constructors, pattern matching, ABI and destruction |
| `match`, `case` | Reserved; pattern grammar absent | Exhaustive pattern matching, guards, bindings and decision-tree lowering |
| `guard` | Reserved; statement grammar absent | Early-exit semantics and definite-initialization integration |
| `block` | Reserved; no independent block construct | Scoped expression/block semantics if retained in the language design |
| `init`, `deinit` | Reserved; no constructor/destructor model | Initialization invariants and deterministic destruction integrated with ownership |
| `throw`, `throws`, `catch`, `rethrow` | Reserved; exception semantics absent | Typed error effects, unwind edges, cleanup and target runtime ABI |
| `async`, `thread` | Token kinds exist but are not reserved by the current keyword mapper | Async functions, tasks, thread APIs, synchronization and coroutine lowering |
| `protocol`, `variant`, `namespace` | Token kinds exist but are not reserved by the current keyword mapper | Protocol/trait conformance, algebraic variants and namespace/module semantics |
| `defer` | Token kind exists but is not reserved by the current keyword mapper | Lexical cleanup stack and control-flow-safe deferred execution |
| `borrow`, `store`, `manual` | Token kinds exist but are not reserved by the current keyword mapper | Remove legacy manual ownership surface; define borrow/store semantics only if needed by the final ownership model |
| `static`, `inline`, `private`, `public` | Token kinds exist but are not reserved by the current keyword mapper | Storage duration, inlining policy and visibility semantics |
| `for`, `in`, `mod`, `set` | Token kinds exist but are not reserved by the current keyword mapper | Decide whether these are language features or obsolete token declarations; implement or remove consistently |

## Backend support policy

The backend matrix must be generated from tested capability declarations, not inferred from the existence of an emitter file.

| Target | Current claim | Required release gate |
|---|---|---|
| x86-64 | Primary native target; typed scalar IR plus AST compatibility path | Full ABI, ownership cleanup, aggregates, exceptions, concurrency and runtime tests |
| AArch64 | Partial scalar SVM emitter | Same supported-feature suite as x86-64 plus AAPCS64 ABI tests |
| ARMv7 | Partial scalar SVM emitter | AAPCS32 ABI, alignment, atomics/runtime and supported-feature suite |
| RISC-V64 | Partial scalar SVM emitter | RISC-V ABI, atomics/runtime and supported-feature suite |
| WASM32 | Partial scalar SVM emitter | WASI/browser host contracts, memory safety, imports/exports and feature suite |
| WASM64 | Partial memory64 emitter | memory64 ABI/runtime and feature suite |
| LLVM | Optional textual IR bridge | Versioned LLVM compatibility, data layout, ABI and link/runtime tests |

A target must reject unsupported features during capability checking with a source-located diagnostic. It must never silently route through an unrelated backend or claim compatibility based on successful assembly emission alone.

## Required implementation sequence

1. **Compiler truth and test harness:** canonical keyword registry, parser acceptance tests, feature-capability manifest, source-located unsupported-feature diagnostics, target matrix CI.
2. **SIL correctness:** typed parameters and locals, load/store bindings, complete expression operand handling, valid CFG for terminated branches, verifier invariants, and executable SIL-to-SVM lowering. AST-to-SVM must stop being the normal lowering path.
3. **Type and aggregate system:** primitive types, pointers/references, tuples, arrays/slices, structs, enums/variants, layout, generic functions/types, protocols and pattern matching.
4. **Ownership and memory:** stack-first values, moves/borrows, escape analysis, heap promotion, deterministic destruction, error/early-return cleanup, and cross-target parity. No routine user-authored `free`.
5. **Control/effect system:** range/iterator loops, closures, defer, typed errors, async/tasks, threads, atomics and synchronization.
6. **SVM and optimization:** richer typed SSA, memory model, vector/SIMD and atomic operations, target-independent pass manager, 350 registered pass slots across stages, pass verification and reproducible profiling.
7. **Backends and ABI:** bring each target to an explicit feature level; complete object/link/runtime paths and conformance tests.
8. **Standard library and ecosystem:** stable core modules for systems, app/service, web/WASM, AI/science, graphics/games and embedded domains; package manager, LSP, formatter and debugger integration.

## Definition of done

A feature is marked **supported** only when all of the following exist:

- Syntax and AST representation.
- Semantic validation and useful negative diagnostics.
- Typed SIL generation and SIL verifier coverage.
- SIL-to-SVM lowering and SVM verifier coverage.
- Backend capability declaration and executable/object-level tests for each claimed target.
- Runtime and standard-library integration where required.
- Optimization correctness tests and profiling.
- Documentation examples that compile in CI.

Until then it must be marked **partial**, **reserved**, or **unsupported** in documentation and diagnostics.
