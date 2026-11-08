# Swan Performance Engineering

## Current architecture

Swan has two x86-64 paths:
- The established AST backend for the broader existing language subset, including checked/unchecked u32 arrays, logging and runtime-lazy imports.
- A verified typed-IR backend preferred for the scalar subset. It emits Swan-owned System V assembly.

The same typed IR is consumed by the AArch64, ARMv7, RISC-V64, WASM32 and WASM64 generators for their documented subsets. SVM executes the IR portably and can load/store a versioned .swvm artifact. No C transpilation or LLVM code-generation fallback is used.

## Implemented compile-time work

- Arena-backed AST storage and compact token storage.
- Hash-indexed function, semantic-symbol and backend-local lookup.
- Typed SSA IR with control-flow, PHI nodes, calls and memory operations.
- IR verifier for SSA bounds/uniqueness, dominance, branch targets, PHI completeness, call signatures and operand types.
- IR generation for integer/boolean expressions, direct calls, mutable locals, branches, loops, loop control and short-circuit logic.
- Verified optimization pass manager: width-aware constant folding, select folding, constant-branch/CFG simplification, unreachable-block removal, algebraic simplification, dominance-aware CSE and iterative DCE.
- Versioned, bounded .swvm serialization and portable SVM execution.
- Dedicated assembly emitter and linker helper interfaces.

## Implemented runtime work

- Existing x86-64 AST backend: register-backed locals, caller/callee-saved register handling, spills, leaf inlining, constant folding, short-circuit logic, power-of-two division lowering, selected loop strength reduction and optimized u32 array indexing.
- IR backends: scalar instruction selection, calls, stack-local memory, CFG and PHI edge copies for supported targets.
- SVM: typed integer semantics, recursive calls, memory, PHI/control flow, fuel and recursion limits.

The IR backends currently use stack/register slots and conservative lowering. They are correctness-oriented and not yet a replacement for the optimized x86-64 AST backend on array-heavy/runtime-critical workloads.

## Remaining performance work before SIL

1. Implement liveness analysis and global register allocation for IR backends.
2. Add copy propagation, loop-invariant code motion, and further global value numbering with dominance-aware tests.
3. Add target-aware instruction selection and scheduling.
4. Add array/slice IR, bounds-check semantics and native lowering.
5. Add floating-point IR semantics, SVM execution and target instruction selection.
6. Add complete import/module lowering and target runtime linking.
7. Expand ABI and execution tests on native/cross-architecture runners.
8. Differentially test the IR x86-64 path against the legacy AST path over the existing source corpus.
9. Measure runtime and compile-time deltas on identical benchmark workloads before adopting IR optimizations in the default fast path.

SIL is intentionally not part of this work. It remains deferred until the typed IR, codegen, artifact and target support boundaries above are stable.
