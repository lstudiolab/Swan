# Swan Engineering Roadmap

This roadmap records the current production compiler and the remaining engineering work. Planned capabilities are not claims of completed support.

## Current baseline

- One production compiler: the C11 implementation in `compiler/tools/swanc/main.c`.
- `build.sh` builds that compiler directly. No bootstrap compiler or second compiler is built.
- UTF-8-aware lexer and indentation-based parser.
- AST semantic analysis, standard-library module loading, limited scalar C-header import, and reachability analysis.
- General typed SSA IR, verifier, direct AST-to-IR lowering, and target-independent optimizer.
- A bounded fixed-point optimizer with a maximum of 350 pass executions.
- x86-64 typed-IR codegen plus AST compatibility codegen for additional implemented features.
- Shared SVM compile core feeding Swan-owned x86-64, AArch64, ARMv7, RISC-V64, WASM32, and WASM64 backends.
- Optional explicit LLVM target-triple bridge.
- Automatic cleanup for eligible local arrays and a stack-array fast path for small fixed non-escaping arrays.

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
- Compiler stage profiling and GitHub Actions benchmark infrastructure, including peak RSS measurement.

## Priority 0: measure and reduce compiler resource use

1. Establish repeatable GitHub Actions baselines for small, medium, and stress-sized Swan programs.
2. Record per-stage and per-optimization-pass time, end-to-end wall time, compiler peak RSS, compiler build time, and generated artifact size.
3. Use the measured bottlenecks to prioritize work; do not guess based on source size or pass count.
4. Reduce avoidable token, AST, IR, and optimizer temporary allocations while preserving ownership and diagnostics.
5. Replace repeated linear symbol/function scans in hot paths with collision-safe indexes where benchmarks show a benefit.
6. Reuse optimizer workspaces and analysis results when their invalidation rules are explicit.
7. Keep the non-profiled path free of timing overhead when profiling is disabled.

## Priority 1: correctness and backend parity

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

A change is not a performance win merely because it reduces source lines, instruction count, or compile time. Measure compile time and runtime separately, validate checksums, report memory use, and record the exact compiler revision and machine configuration. Compare only equivalent workloads and optimization settings. Do not claim superiority over C, Rust, Goose, or other languages without reproducible, current results.
