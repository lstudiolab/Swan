# Compiler Performance

## Current pipeline

`.swan -> Lexer -> Parser -> AST -> Semantic analysis -> x86-64 assembly -> assembler/linker`

Swan now generates assembly directly. The C source-generation stage has been removed from the active compilation path. The system toolchain is used to assemble and link the emitted instructions, not to translate generated C.

## Native backend baseline

The first backend is a correctness-oriented x86-64 System V ELF implementation. It currently uses stack slots for locals and parameters and emits explicit control-flow labels, arithmetic, comparisons, function calls, and libc output calls.

This is a baseline, not the final optimization strategy. Direct assembly alone does not guarantee faster code than optimized C. In particular, the current backend does not yet perform register allocation, loop-invariant code motion, strength reduction, or machine-level instruction scheduling.

## Optimization priorities

1. Measure generated instruction counts and runtime hot paths.
2. Add a typed low-level IR before complex machine optimizations.
3. Introduce liveness analysis and register allocation to reduce stack traffic.
4. Add constant folding, copy propagation, dead-code elimination, and common-subexpression elimination.
5. Add loop optimizations and target-specific instruction selection.
6. Validate every optimization against the same correctness suite and benchmark checksums.

## Measurement policy

Record compiler revision, target, CPU, assembler/linker versions, and flags. Use fixed small, medium, and large fixtures; repeat trials and report medians. Keep frontend/assembly emission, assemble/link, end-to-end compile, and prebuilt runtime measurements separate. Never compare timings unless the workload and result are identical.

Correctness and generated-code equivalence take precedence over compile-time improvements.