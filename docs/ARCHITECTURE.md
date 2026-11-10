# Compiler Architecture

Swan owns its frontend, typed intermediate representation, and optimization passes. Its optimized native path uses Swan-owned machine-code backends. An optional LLVM IR portability backend serializes verified Swan IR and invokes Clang/LLVM for targets and object formats outside the native backend matrix; it is not used by default.

## Implementation model

The compiler is C11. `main.c` is the only compiled translation unit; compiler subsystems are headers under `compiler/include/swan/`.

## Active compilation paths

The x86-64 compiler has two active paths:

```text
.swan -> lexer -> parser / AST -> imports -> semantic analysis
  -> typed SSA IR -> verified scalar optimizations -> Swan x86-64 IR backend
  -> assembly emitter -> system assembler/linker
```

The compatibility path still lowers constructs not yet represented in IR (including the existing array, logging, and runtime-lazy import implementation) directly from the AST to Swan-generated x86-64 assembly.

Cross-target AArch64, ARMv7, RISC-V64, WASM32, WASM64, and SVM consume the documented typed-IR subset. The optional LLVM IR bridge accepts arbitrary LLVM target triples (subject to the installed Clang target build) and can emit target assembly or relocatable objects. All cross-target routes currently consume the typed-IR subset; this does not imply full language or runtime parity.

## Implemented compiler components

- Lexer, indentation-aware parser, AST, import resolution, and semantic analysis.
- Typed SSA IR with integer widths, booleans, pointers, arithmetic, comparisons, branches, PHI nodes, calls, and local memory.
- IR verifier for SSA definitions, use-before-definition, dominance, CFG edges, PHI predecessor coverage, operand types, and call signatures.
- Verified optimization passes: width-aware constant folding, constant-select folding, CFG simplification, algebraic simplification, trivial-PHI copy propagation, dominance-aware common-subexpression elimination, conservative loop-invariant code motion, and iterative dead-code elimination.
- Portable `.swvm` artifact serialization with versioning, bounded reads, malformed-input checks, and explicit ownership of decoded strings.
- SVM execution with typed integer semantics, calls, local memory, PHI/CFG execution, fuel limits, and call-depth limits.
- Swan-owned x86-64 System V AST compatibility and typed-IR code generation.
- Swan-owned AArch64, ARMv7, RISC-V64, WASM32, and WASM64 typed-IR emitters.
- Separate assembly emission and linker-driver interfaces.

## Current limitations

The IR generator does not yet lower array/slice operations, strings and logging, floating-point arithmetic, closures, aggregate values, or runtime-lazy imports. Those constructs continue to use the x86-64 compatibility path where supported; unsupported target/language combinations must fail with explicit diagnostics.

IR backends currently use conservative stack/register slots. A global liveness-driven register allocator, spill insertion, loop-invariant code motion, broader target instruction selection, and complete cross-target runtime integration remain unfinished.

SAMC currently provides conservative local array cleanup in the AST backend. Escaping arrays are deliberately not implicitly released; interprocedural ownership and per-block last-use analysis remain future work.

## Target and ABI boundaries

- x86-64 currently targets the System V Linux ABI.
- AArch64 targets the AAPCS64 Linux ABI.
- ARMv7 targets the AAPCS32 ARM-state Linux ABI and uses EABI division helpers for integer division/remainder.
- RISC-V64 targets the documented Linux integer ABI subset.
- WASM32 emits a WebAssembly 32-bit memory module.
- WASM64 emits a WebAssembly memory64 module; runtime interoperability depends on a compatible Memory64 runtime.
- SVM executes verified typed IR as a portable virtual machine.

The hand-written backends remain Linux-focused. Windows COFF, Apple Mach-O, additional ARM profiles, and other LLVM-supported targets can be selected through `--target-triple`, provided the installed Clang build supports them. This path emits relocatable objects or assembly; final executable linking and platform runtime integration are separate steps.

## Engineering rules

1. Never transpile Swan source into C.
2. Keep the default native backends Swan-owned. LLVM/Clang may be used only through the explicit `--target-triple` portability path.
3. System assemblers and linkers may consume Swan-generated assembly.
4. Never claim full target parity based only on successful object/module emission.
5. Every optimization requires correctness tests and reproducible performance measurements.
6. Keep the pre-SIL IR and compatibility pipelines stable before beginning another IR layer.
