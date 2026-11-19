# Compiler Completion Matrix

This matrix distinguishes implemented infrastructure from language-feature completeness.
A parser token, IR opcode, or successful object emission is not evidence of end-to-end
support. Status is based on the production source and the repository's documented
implementation boundaries; it is not a claim that every path has been runtime-tested.

**Status definitions**

- **Complete (scoped):** implemented and tested within the explicitly stated boundary.
- **Partial:** a working subset exists, but important language, target, or runtime cases remain.
- **Not implemented:** no complete production implementation is documented.
- **Validation needed:** implementation exists, but BrewRuntime/cross-target evidence is insufficient.

## Frontend and project model

| Subsystem | Status | Implemented | Still required |
|---|---|---|---|
| Source loading and UTF-8 validation | Complete (scoped) | UTF-8 validation, BOM handling, source buffers and diagnostics | Full Unicode identifier XID tables and normalization |
| Lexer | Complete (scoped) | Current token set, indentation and UTF-8 byte preservation | Unicode normalization and exhaustive fuzz/property tests |
| Parser / AST | Partial | Indentation-based grammar and AST for current syntax | End-to-end implementation of all advertised constructs; grammar fuzzing |
| Semantic analysis | Partial | Existing declaration, type, control-flow and diagnostic checks | Complete struct/member/method resolution, full type inference and type-compatibility diagnostics |
| Imports / modules | Partial | Brew module loading, namespaces, lazy imports and C-header scalar declarations | Complete package resolution, cycles/visibility policy, portable native ABI and dependency graph validation |
| Reachability | Partial | Function reachability from `main` for supported direct-call forms | Indirect calls, closures, address-taken functions and complete module graph semantics |
| Diagnostics | Partial | Source positions, phase errors, warnings and warnings-as-errors | Structured diagnostic codes, robust imported-source spans and error recovery |

## Intermediate representations

| Subsystem | Status | Implemented | Still required |
|---|---|---|---|
| Typed BVM IR data model | Partial | Typed SSA values, blocks, operations, calls, PHIs and memory operations | Complete source lowering for every supported Brew type and construct |
| IR generation | Partial | Integer/boolean scalar subset, direct calls, locals, assignment, if/else, loops and short-circuit logic | Arrays, strings, floats, aggregates, member access, indirect calls and full import lowering |
| IR verifier | Complete (scoped) | SSA/dominance, PHI placement, operation contracts, signatures and malformed storage checks | Expand adversarial/property tests as new operations are added |
| IR optimizer | Complete (scoped) | Constant folding, CFG simplification, algebraic simplification, PHI propagation, CSE, conservative LICM and DCE | More analyses/passes, compile-time budgets, differential testing and proof of semantics preservation |
| SIL | Partial / integration unclear | Repository references SIL in build messaging and memory-model plans | A single documented, independently verifiable SIL contract; ARC retain/release lifetime analysis, verifier, lowering and integration tests |
| BVM common pipeline | Partial | Shared verification/optimization and backend dispatch | One mandatory path for all source features; retire the AST compatibility route |
| BIC environment detection | Complete (scoped) | Detects WebAssembly-hosted builds and native x86-64/ARM64 hosts; selects browser-WASM or native Wasmtime execution | End-to-end browser integration tests and broader host/OS detection matrix |
| Portable WASM64 AOT command | Complete (scoped) | `brewc compile` emits a WASM64 module using the existing backend | Expand source-feature coverage and memory64 browser interoperability |
| Native runtime JIT | Partial / validation needed | Native WASM execution delegates to Wasmtime, which JIT-compiles for the host CPU; CI now executes WASM64 on native ARM64 | Keep x86-64/ARM64 runtime conformance, startup/cache benchmarks, and broader import coverage |
| Browser WASM execution | Partial / validation needed | Browser loader instantiates the portable module without Brew native recompilation | Validate memory64 support and imports across target browsers |
| Serialization / bytecode runtime | Not implemented by design | BVM is the compiler IR; WASM64 is the portable executable artifact | No Brew bytecode interpreter or dispatch loop is required |

## Memory and language semantics

| Subsystem | Status | Implemented | Still required |
|---|---|---|---|
| Compiler-managed array cleanup | Partial | Eligible x86-64 AST-path local arrays, stack placement, conservative last-use cleanup and early-return cleanup | Shared lifetime model in SIL/typed IR and all backends |
| Automatic reference counting (ARC) | Partial | Limited compiler-generated cleanup for selected x86-64 array forms | Retain/release insertion, strings/aggregates/closures/collections, cycle strategy, error-path cleanup and cross-target parity |
| Managed lifetime analysis | Partial | Conservative array escape handling and last-use cleanup in x86-64 compatibility path | Target-independent managed-reference analysis and complete ARC dataflow |
| Structs / aggregates | Not implemented end-to-end | Syntax vocabulary and partial representation | Layout, field access, initialization, destruction, alignment and ABI passing/return |
| Strings / slices | Partial | Existing frontend/runtime compatibility functionality | Typed IR representation, ownership/lifetime, slicing and cross-target runtime |
| Floating point | Partial | IR type declarations and limited backend representation | Source literal/type semantics, arithmetic lowering, conversions, ABI and runtime tests |
| Exceptions / unwinding | Not implemented end-to-end | Keyword vocabulary exists | Defined semantics, unwind tables/runtime, cleanup integration and target support |
| Closures / function values | Not implemented | None complete in the production pipeline | Capture analysis, environment layout, indirect calls and lifetime rules |

## Backends and toolchain

| Target / stage | Status | Implemented | Still required |
|---|---|---|---|
| x86-64 System V | Partial but most complete | Native assembly/executable; signed i32/i64 arithmetic, shifts, division traps, direct calls and scalar C ABI; AST compatibility features | Unified typed-IR feature coverage, aggregate/float ABI, robust edge cases and expanded sanitizer/runtime matrix |
| AArch64 / ARM64 | Partial | Native ARM64 executable linking/running; cross-target ELF object/assembly; signed i32/i64 arithmetic, calls, and stack-passed arguments; scalar C ABI linking on native ARM64 | Full type/operation coverage, aggregate and floating-point ABI, broader FFI, cross-target linking and standard-library parity |
| WASM64 | Partial | Memory64 module emission; scalar i32/i64 arithmetic and control flow; signed division overflow traps; Wasmtime execution tested on x86-64 and ARM64 hosts | Browser memory64 interoperability, imports, complete memory model and broader language coverage |
| Instruction selection | Partial | Target-specific lowering for current supported operations | Legalization and coverage for all operations/types |
| Register allocation | Partial | Backend-specific register/local assignment for supported subset | Proven global allocation, spill/reload correctness and stress tests |
| Machine IR | Not established as a complete shared stage | Target backend internal representations | Shared machine IR contract, verifier and target-independent passes |
| ABI lowering | Partial | Scalar System V and AAPCS64 calls; i32/i64 arguments and returns; ARM64 stack-passed arguments and native scalar C linking | Aggregates, floats, variadics, complete callee-save/stack rules, and broad external ABI conformance |
| Object emission | Partial | Brew-owned ELF and WASM64 outputs | Relocations, debug info, object-format edge cases and validation per target |
| Linking / runtime | Partial | Native x86-64 and ARM64 executable linking; native ARM64 scalar C-object linking; WASM64 execution through Wasmtime | Cross-target x86/ARM linkers and sysroots, platform runtimes, and broader executable tests |

## Standard library and runtime libraries

| Module / subsystem | Status | Implemented | Still required |
|---|---|---|---|
| `std` | Partial | Signed 64-bit helpers for bounds, sign, powers, factorial, triangular sums and averages; checked multiplication and logarithmic-time exponentiation; overflow sentinel is `0` | Structured arithmetic errors, floating-point APIs, strings, formatting and broader numeric types |
| `foundation` | Compatibility subset | Legacy `*_int` helper names with the same overflow behavior as `std` | This is not a platform Foundation framework; consolidate or deprecate duplicated APIs |
| `std_math` | Partial | Integer GCD/LCM, primality, division helpers, integer square root, Fibonacci, triangular numbers and power-of-two helpers | Floating-point math, transcendental functions, richer error results and high-volume algorithm tests |
| `std_collections` | Partial | Checked `u32_array` queries, mutation, linear/binary search, bubble/selection/insertion/heap sort, negative-length guards and sum-overflow protection | Generic `Array<T>`, dynamic capacity, slices, maps/sets, iterators, queues/stacks and ownership-aware storage |
| JSON C runtime | Partial | Native C ABI parser, document handles, key lookup and compact/pretty writer | Typed Brew JSON values, streaming APIs, full pointer/string safety and portable ABI |
| HTTP C runtime | Partial | Bounded HTTP/1.1 JSON echo/health service and POSIX worker pool | User-defined routes, HTTP client, TLS, chunked transfer, keep-alive, WebSockets and async I/O |
| Brew string/bytes libraries | Not implemented end-to-end | String literals and limited compatibility/runtime paths | Managed string/byte types, Unicode operations, slices, encoding and cross-target ownership |
| I/O, filesystem, process and time libraries | Not implemented | No complete Brew source modules | Portable file/directory APIs, process/environment access, clocks, timers and error handling |
| Networking, HTTP client and database libraries | Not implemented | Fixed native HTTP server only | TCP/UDP, TLS, HTTP client, SQLite/PostgreSQL/MySQL drivers, pools, transactions and cancellation |
| Async, synchronization and task libraries | Not implemented | Native POSIX thread-pool primitive | Brew tasks, async/await, scheduler, channels, cancellation and structured concurrency |
| Encoding, crypto, regex, CSV and formatting | Not implemented | No complete Brew modules | Safe, portable source APIs with cross-target tests |
| Logging, testing and documentation libraries | Not implemented | Compiler diagnostics and shell-based test harness | Brew test framework, structured logging, formatter and documentation generator |

The integer helper APIs currently use `0` as an overflow or invalid-input sentinel in several places. This is documented, but it is inherently ambiguous when zero is also a valid result. A future structured error/result type is required before these APIs can provide unambiguous failure reporting.

## Driver, quality and release engineering

| Subsystem | Status | Implemented | Still required |
|---|---|---|---|
| CLI / phase gating | Complete (scoped) | Build/run/emit modes, targets, nonzero failure behavior | Consistent diagnostics and option coverage for every project mode |
| Multi-file compilation | Partial | Per-source x86-64 objects and project-level cross-target IR output | Consistent separate compilation and symbol/ABI rules on every target |
| Test suite | Partial | Regression, verifier, lexer, FFI, module, standard-library and architecture scripts | Run all tests in CI; target runtime matrix, fuzzing, sanitizers and differential tests |
| Fuzzing | Not established | No comprehensive fuzz campaign documented | Lexer/parser/IR/optimizer fuzzers and minimized regression corpus |
| Sanitizers | Partial | Build script supports ASan/UBSan configuration | Execute full suite under sanitizers in CI and add leak checks |
| Benchmarks | Partial | Profiling and per-pass timing | Stable benchmark corpus, baselines and regression thresholds |
| Compiler implementation language | Out of scope by design | Brew compiler is implemented in C11 using an existing toolchain | No self-hosting or Brew-written bootstrap compiler is required |
| Editor tooling | Partial | VS Code syntax highlighting, indentation and snippets | Language server, diagnostics, completion, formatting and navigation |

## Completion gates

A subsystem should only be marked complete when all applicable gates pass:

1. Syntax and semantic positive tests.
2. Negative tests proving invalid programs fail with source diagnostics.
3. IR generation and verifier tests.
4. Optimized and unoptimized behavior equivalence.
5. Backend assembly/object validation for every target claiming support.
6. Runtime execution tests where a target runner is available.
7. ABI interoperability tests for external calls and linked modules.
8. ASan/UBSan and leak checks for native compiler/runtime paths.
9. Fuzz/property tests for parsers, verifiers and transformations.
10. Documentation that matches the tested implementation boundary.

## Current conclusion

Brew has a real production compiler, typed SSA infrastructure, an optimizer, and multiple
code-generation backends. It is not yet feature-complete or cross-platform equivalent.
The largest architectural blockers are the AST compatibility path, incomplete typed-IR
lowering, incomplete ARC integration, incomplete aggregate and floating-point semantics,
and limited cross-target ABI/runtime validation.

Do not describe the language as having complete ARC or full WORA until those gates pass
on every supported target. ARC is the only ordinary source-level memory model; a raw mode
and manual ownership syntax are explicitly out of scope.
