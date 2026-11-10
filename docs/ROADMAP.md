# Swan Engineering Roadmap

This roadmap separates implemented work from remaining engineering. It is not a claim that every planned item is already supported.

## Current baseline

- C11 compiler host with a Swan-authored SIL scalar subsystem compiled during bootstrap.
- UTF-8-aware lexer and indentation-based parser.
- AST semantic analysis, standard-library module loading, limited scalar C header import, and reachability analysis.
- General typed SSA IR, verifier, AST-to-IR lowering, and target-independent optimizer.
- x86-64 typed-IR codegen plus AST compatibility codegen for additional implemented features.
- Swan-owned AArch64, ARMv7, RISC-V64, WASM32, WASM64, and SVM typed-IR backends.
- Optional explicit LLVM target-triple bridge.
- Strict local ownership checking for `manual var`, SAMC cleanup for eligible local arrays, and a stack-array fast path for small fixed non-escaping arrays.
- SVM in-memory execution, versioned `.swvm` artifacts, fuel limits, and per-frame lazy alloca initialization.

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
- SVM lazy local-slot initialization by frame generation.
- x86-64 compatibility-path register locals, call preservation, leaf-frame elimination, small-function inlining, arithmetic strength reduction, and specialized array access/allocation paths.

## Priority 1: correctness and parity

1. Keep the full test suite green, including C ABI, UTF-8, manual ownership, legacy syntax, lazy imports, SVM, and target emission.
2. Add differential execution tests for equivalent programs compiled through typed-IR and AST compatibility paths.
3. Expand negative tests for unsupported source constructs and backend operation/type combinations.
4. Verify ABI behavior with executable tests on native runners for AArch64, ARMv7, and RISC-V64.
5. Validate WASM32 and WASM64 behavior with suitable runtimes, not only binary inspection.

## Priority 2: runtime performance

1. Profile typed-IR generated native programs and identify hot instruction-selection and value-storage paths.
2. Implement liveness analysis and a global register allocator for typed-IR machine-code backends.
3. Add spill insertion and register-pressure tests.
4. Improve target-specific instruction selection and scheduling only where measurements justify it.
5. Expand typed IR to arrays/slices and bounds-check semantics without regressing the compatibility backend.
6. Add floating-point execution semantics and backend coverage.
7. Compare SVM dispatch and memory-access strategies against native execution on identical workloads.

## Priority 3: language completeness

1. Define complete static typing and type inference rules.
2. Implement struct layout, field access, method resolution, and aggregate ABI lowering.
3. Define pointer/reference and ownership-transfer semantics before implementing borrowing.
4. Add string values and general output to typed IR.
5. Expand imports and runtime module linking across targets.
6. Turn recognized-but-unimplemented keywords into explicit feature work or remove/deprecate them from the public language surface.

## Performance policy

A change is not a performance win merely because it reduces source lines, instruction count, or compile time. Measure compile time and runtime separately, validate checksums, report memory use, and record the exact compiler revision and machine configuration. Do not claim superiority over C, Rust, or other languages without reproducible, current results.
