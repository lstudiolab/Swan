# Swan Compiler and Runtime Performance

This document records optimizations present in the implementation. It does not claim a speed ranking over C, Rust, Goose, or other languages. Performance claims require reproducible measurements on identical workloads.

## Compile-time architecture

Swan reduces frontend and code-generation overhead with:

- Arena-backed AST allocation and bulk teardown.
- Compact token records backed by shared token text storage.
- AST nodes that allocate argument vectors only for call expressions.
- Token-text references for names and types rather than repeated large fixed-size copies.
- Hash-indexed function and scoped-symbol lookup.
- Indexed backend-local lookup.
- Length-dispatched keyword recognition in the lexer.
- Cached expression call/constant metadata where repeated analysis would otherwise walk expression trees.
- Compact per-loop write sets for loop-invariant analysis.
- Low-overhead assembly emission that writes literal instruction strings directly and formats only when needed.
- Cached call analysis and conservative analysis reuse in the AST backend.

These are implementation techniques; their individual speedups have not been stated without controlled measurements.

## Typed-IR optimization

The typed-IR optimizer currently performs:

- Width-aware integer constant folding.
- Constant-select folding.
- Constant branch and CFG simplification.
- Unreachable-block removal.
- Algebraic simplification.
- Trivial-PHI copy propagation.
- Dominance-aware hash-indexed common-subexpression elimination with one-pass SSA use rewriting.
- Conservative loop-invariant code motion with cached definition-block lookups and stable, iterative hoist batches.
- CSE after loop hoisting.
- Worklist-based dead-code elimination.

DCE builds value liveness from observable roots and propagates dependencies through SSA definitions. It avoids repeatedly scanning all instructions until no change. Potentially trapping division/remainder and memory operations are not treated as removable pure instructions. SSA value types are cached by value ID, making type queries constant-time instead of repeatedly walking every block. The optimizer always verifies IR at pipeline entry and exit; debug builds additionally verify every pass boundary, while release builds avoid those repeated whole-module verification walks.

The SVM compile core runs the optimizer for every target using typed IR. The x86-64 AST compatibility backend retains its own constant folding, inlining, register-local, and loop/array optimizations; it does not automatically receive every typed-IR pass.

## Recent measured optimizer improvements

The typed-IR optimizer was measured on the Ubuntu 24.04 x86-64 GitHub runner using the pipeline stress benchmark (two warmups, median of five measured runs). Comparing revisions `21df015d` and `0dd86aef`:

| Workload | IR optimization before | IR optimization after | Change |
|---|---:|---:|---:|
| Expression/call graph (256 functions) | 2.475 ms | 2.120 ms | 14.4% lower |
| Control flow (64 functions) | 0.487 ms | 0.299 ms | 38.7% lower |
| Deep call chain (512 functions) | 1.303 ms | 1.247 ms | 4.3% lower |

The later revision replaced instruction-wide CSE searches with a dominance-aware hash table and replaced repeated copy-use rewrites with a single SSA replacement-map rewrite. These are stage timings only; they are not application runtime or whole-compilation speedups. Results are specific to these generated workloads and this runner class.

## Runtime and backend fast paths

The x86-64 AST compatibility backend includes register-backed locals, caller/callee-saved register handling, spill support, leaf-frame elimination, small-function inlining, constant folding, short-circuit logic, power-of-two division/remainder lowering, selected loop strength reduction, and specialized u32 array indexing/bounds-check elimination when safety can be proven.

For arrays, compile-time-known lengths avoid repeated header loads; constant in-range indices can use direct base-plus-displacement accesses. Constant-size allocation avoids unnecessary runtime length evaluation. A recent stack-array fast path places small fixed-size arrays in the native stack frame only when they do not escape.

Compiled execution no longer performs IR instruction dispatch. Each selected backend emits target instructions or a target module; runtime performance is determined by generated code, ABI lowering, register allocation, and the target runtime.

The typed-IR native backends currently use conservative value/stack slots and are not yet equivalent to a global liveness-driven register allocator with spill insertion and instruction scheduling.

## Ownership lifetime lowering

The x86-64 compatibility backend now initializes eligible heap-array handles to null before control flow can bypass their declarations. Generated cleanup checks each handle and clears it after release, making cleanup safe across conditional allocation and idempotent across return and epilogue paths.

For non-escaping local arrays, a conservative AST liveness walk emits release after the final top-level statement that can reference the array. Branches and loops are treated as one enclosing statement, so a value used inside either construct remains live until that construct completes. Small fixed-size non-escaping arrays continue to use stack-frame storage and do not require a heap free.

This is an incremental lifetime optimization, not the completed Rust-style ownership system. Escaping arrays are deliberately excluded from this release pass until ownership transfer and borrow checking are represented explicitly. The legacy manual allocation modifier and source-level free intrinsic have been removed from the C11 parser and semantic checker. The typed-IR backends do not yet share the AST array ownership implementation; general ownership transfer, borrowing, destructor execution, and exception unwinding remain incomplete.

## Profiling and measurement

The CLI supports `--profile` and reports stage durations for source reading, lexing, parsing, imports, semantic analysis, reachability, IR generation, IR optimization, code generation, assembly output, native linking, and execution when applicable.

Run the compiler pipeline benchmark:

```sh
sh build.sh
python3 benchmarks/compiler_pipeline.py
```

It reports medians for the included compilation workloads and stages. Use `--repeats N` and `--warmups N` to control sample counts.

The runtime benchmark suite includes binary search and other language-comparison workloads. Before comparing performance, record compiler revision, CPU, OS, toolchain versions, optimization flags, workload parameters, memory use, and output checksum. Separate compilation time from runtime. Compare the same algorithm and validate identical results.

## Remaining engineering priorities

1. Add differential execution tests between typed-IR x86-64 and AST compatibility paths where feature overlap exists.
2. Add liveness analysis and global register allocation for IR backends.
3. Improve target instruction selection and scheduling after profiling identifies bottlenecks.
4. Expand typed IR to cover arrays/slices and string/output semantics.
5. Expand floating-point semantics and backend execution coverage.
6. Complete cross-target runtime linking and ABI tests on native runners.
7. Measure each optimization independently; remove complexity that does not produce a repeatable benefit.

Do not describe Swan as faster than another compiler without current, reproducible benchmark evidence.
