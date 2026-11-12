# Swan Engineering Roadmap

This roadmap separates implemented work from remaining engineering. It is not a claim that every planned item is already supported.

## Current baseline

- Exact 36-word reserved-keyword contract shared by the C11 bootstrap lexer and Compiler 2 lexer, with regression tests for non-keyword legacy spellings.

- C11 compiler host with a Swan-authored SIL scalar subsystem compiled during bootstrap.
- UTF-8-aware lexer and indentation-based parser.
- AST semantic analysis, standard-library module loading, limited scalar C header import, and reachability analysis.
- General typed SSA IR, verifier, AST-to-IR lowering, and target-independent optimizer.
- x86-64 typed-IR codegen plus AST compatibility codegen for additional implemented features.
- Shared SVM compile core feeding Swan-owned x86-64, AArch64, ARMv7, RISC-V64, WASM32, and WASM64 backends.
- Optional explicit LLVM target-triple bridge.
- Automatic cleanup for eligible local arrays and a stack-array fast path for small fixed non-escaping arrays; full move/borrow analysis is still a Compiler 2 release blocker.

## Completed optimization work

- Arena-backed AST allocation and compact token storage.
- Hash-indexed function and scoped-symbol lookup.
- Typed SSA IR and structural/type verification.
- Width-aware constant folding and constant-select folding.
- CFG/constant-branch simplification and unreachable-block pruning.
- Algebraic simplification and trivial-PHI copy propagation.
- Dominance-aware CSE.
- Conservative loop-invariant code motion and post-hoisting CSE.
- Worklist-based dead-code elimination.
- x86-64 compatibility-path register locals, call preservation, leaf-frame elimination, small-function inlining, arithmetic strength reduction, and specialized array access/allocation paths.

## Priority 0: make Compiler 2 executable and safe

Compiler 2 (swanc/) is not yet a complete compiler. Compiler 1 remains the bootstrap and production implementation until Compiler 2 passes the self-hosting contract in swanc/README.md.

Current Compiler 2 executable slices are the UTF-8 lexer, integer-expression Pratt parser, linear typed-IR verifier/optimizer, and linearized ownership-event verifier. The ownership verifier enforces one-owner state transitions and shared/exclusive borrow conflicts, but AST/CFG event generation, path-sensitive joins, lifetime inference, alias provenance, and automatic drop insertion are not implemented. Its regression suite is tests/swanc-ownership.sh.

Required completion sequence:
1. Implement complete source-file AST parsing and AST structural verification.
2. Implement symbol resolution, canonical type checking, definite initialization, and control-flow graph construction.
3. Generate ownership events from the CFG; implement branch/loop state joins, non-lexical borrow regions, move-path tracking, and deterministic drop elaboration.
4. Implement verified SILGen, SIL ownership verification, demand analysis, and semantics-preserving SIL passes.
5. Implement IRGen and complete typed IR verification, then SVM generation, legalization, liveness, register allocation, spilling, and SVM verification.
6. Implement target-specific ABI/data-layout handling, native/WASM emitters, safe linker invocation, and explicit unsupported-feature errors.
7. Wire the driver so any failed verifier, semantic error, unsupported construct, or unsafe operation stops the pipeline before code generation.
8. Bootstrap Compiler 2 with Compiler 1, then rebuild Compiler 2 with itself and compare deterministic outputs. Run the same positive, negative, fuzz, regression, and target suites at both stages.

## Release gates

- No placeholder-only stage may be reachable from a production build.
- No compiler stage may continue after a prior-stage error or failed verifier.
- All ownership paths must have tests for branch joins, loops, early returns, aliasing, move, borrow conflicts, use-after-move, and automatic drops.
- Fuzz the lexer, parser, AST verifier, IR/SIL/SVM verifiers, and target emitters with sanitizers enabled.
- Run cross-target executable tests where native runners are available; object emission alone is not execution validation.
- Record compile wall time, compile peak RSS, runtime, runtime peak RSS, artifact size, and checksums for identical Swan/Rust/C workloads.
- Treat lower-than-Rust peak memory as an empirical target, not a guaranteed property; report medians and variance across repeated runs.
- Do not cut a production release until stage-2/stage-3 self-hosting and all required release gates pass.

## Priority 1: correctness and parity

1. Keep the full test suite green, including C ABI, UTF-8, automatic array lifetime, contextual compatibility syntax, lazy imports, typed IR, and all target emission.
2. Add differential execution tests for equivalent programs compiled through typed-IR and AST compatibility paths.
3. Expand negative tests for unsupported source constructs and backend operation/type combinations.
4. Verify ABI behavior with executable tests on native runners for AArch64, ARMv7, and RISC-V64.
5. Validate WASM32 and WASM64 behavior with suitable runtimes, not only binary inspection.

## Priority 2: generated-code performance

1. Profile typed-IR generated native programs and identify hot instruction-selection and value-storage paths.
2. Implement liveness analysis and a global register allocator for typed-IR machine-code backends.
3. Add spill insertion and register-pressure tests.
4. Improve target-specific instruction selection and scheduling only where measurements justify it.
5. Expand typed IR to arrays/slices and bounds-check semantics without regressing the compatibility backend.
6. Add floating-point execution semantics and backend coverage.
7. Compare generated native code against reference implementations on identical workloads and validate target-specific ABI behavior.

## Priority 3: language completeness

1. Define complete static typing and type inference rules.
2. Implement struct layout, field access, method resolution, and aggregate ABI lowering.
3. Define pointer/reference and ownership-transfer semantics before implementing borrowing.
4. Add string values and general output to typed IR.
5. Expand imports and runtime module linking across targets.
6. Turn recognized-but-unimplemented keywords into explicit feature work or remove/deprecate them from the public language surface.

## Performance policy

A change is not a performance win merely because it reduces source lines, instruction count, or compile time. Measure compile time and runtime separately, validate checksums, report memory use, and record the exact compiler revision and machine configuration. Do not claim superiority over C, Rust, or other languages without reproducible, current results.
