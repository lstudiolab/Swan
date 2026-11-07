# Compiler Performance

## Current pipeline

`.swan -> Lexer -> Parser -> semantic analysis -> typed AST -> LLVM IR -> LLVM target backend -> assembly/linker`

Swan lowers the checked AST directly to LLVM IR. It does not generate C or invoke a C compiler on generated source. Clang is used as the LLVM driver to select the requested machine backend and emit assembly or link a native executable.

## Native backend baseline

The LLVM backend currently provides target-aware instruction selection, register allocation, and machine optimization for installed LLVM targets. Swan's frontend remains independent of LLVM target-specific details.

The first native IR implementation prioritizes correctness and broad target coverage. It is not yet a complete optimizing IR framework owned by Swan. Optimization passes are currently supplied by LLVM according to the selected optimization level.

## Optimization priorities

1. Validate IR generation against every semantically accepted Swan construct.
2. Add explicit typed IR verification and stable IR-level regression fixtures.
3. Profile generated code and benchmark runtime on each target architecture.
4. Add Swan-owned constant folding, propagation, and dead-code elimination where they improve compile time or generated code.
5. Track compile time, peak memory, object size, and runtime separately.
6. Keep architecture-specific ABI and runtime support explicit.

## Measurement policy

Record compiler revision, target triple, CPU, LLVM version, linker, sysroot, and optimization flags. Use identical workloads and result checksums. Report frontend/lowering, backend codegen, link, end-to-end compile, and prebuilt runtime separately.

Correctness and generated-code equivalence take precedence over compile-time improvements.
