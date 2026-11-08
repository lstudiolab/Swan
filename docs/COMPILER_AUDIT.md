# Compiler Correctness Audit

## Active implementation

- C11 compiler; `main.c` is the only compiled translation unit.
- Header-only compiler modules under `compiler/include/swan/`.
- AST frontend and semantic analysis.
- Typed SSA IR, verifier, source-to-IR lowering and verified optimization passes.
- Production x86-64 System V IR backend for the lowered scalar subset, with the established AST backend retained as a compatibility path for arrays, logging and runtime-lazy imports.
- Swan-owned AArch64, ARMv7, RISC-V64 and WebAssembly code generators for explicitly supported IR subsets.
- Portable register-based SVM interpreter and versioned `.swvm` artifact.
- Dedicated assembly emitter and host linker helpers.
- No C transpilation or LLVM code-generation fallback. Clang is used as an assembler driver for cross-target Swan assembly.

## Implemented source-to-IR subset

- Signed integer and boolean expressions.
- Function parameters, direct calls and recursion.
- Immutable and mutable local bindings.
- Assignment, if/else, while, break and continue.
- Short-circuit AND/OR.
- Scalar arithmetic, signed/unsigned division and remainder, bitwise operations, shifts, comparisons, select, PHI and local memory operations at the IR level.
- Width-aware integer semantics in SVM and constant folding.

## Implemented target output

- x86-64 System V Linux: executable/assembly for the IR subset; legacy AST compatibility path handles currently supported arrays, logging and runtime-lazy imports.
- AArch64 Linux: ELF object/assembly for i1/i64 scalar IR subset.
- ARMv7: ELF object/assembly for i32/i1 subset; integer division is explicitly rejected pending runtime helpers.
- RISC-V64 Linux: ELF object/assembly for scalar integer IR subset.
- WASM32: WebAssembly module with tested scalar control flow, calls, recursion and linear-memory locals.
- WASM64: memory64 module emission for its scalar subset; runtime execution depends on a memory64-capable engine.
- SVM: portable serialized module and interpreter.

Cross-target tests validate emitted ELF architecture and WebAssembly runtime behavior. AArch64/ARMv7/RISC-V64 are currently object-emission tests, not native execution tests on those CPU architectures.

## Known limitations

- IR generation does not yet cover array/slice expressions, strings, logging, floating-point expressions, closures, aggregates, general pointer/reference semantics or all frontend constructs.
- x86-64 IR backend currently supports i1/i64 values and pointers; the legacy AST path provides broader language compatibility.
- AArch64 backend currently supports i1/i64 scalar values and pointers.
- ARMv7 backend currently supports i32/i1 scalar values and pointers; division/remainder need runtime helpers.
- RISC-V64 backend currently supports its implemented scalar integer subset; floating-point and broader ABI cases remain unavailable.
- WebAssembly backend is a deliberately bounded scalar subset; WASM64 runtime interoperability is not yet verified on a memory64 engine.
- Cross-target runtime linking, standard-library import bundling, OS-specific system calls, Windows x64 and Apple Darwin ABIs are not implemented.
- The register-slot IR backends are correctness-oriented; they are not yet globally register-allocated or instruction-scheduled. The mature x86 AST backend remains the performance path for its broader supported feature set.
- Floating-point execution, structs, member access, closures, exceptions and full aggregate ABI lowering are not implemented.
- Static type checking and declared type annotations are still incomplete in the frontend.

These limitations must remain explicit. The compiler must not silently fall back to LLVM, Clang code generation, or C transpilation.

## Required tests

Every new construct requires positive and negative source tests, diagnostic checks, generated-assembly/object inspection where relevant, and executable tests on an appropriate runner. Each target must be described by the exact subset its tests exercise.
