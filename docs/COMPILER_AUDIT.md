# Compiler Correctness Audit

## Current state

The compiler emits assembly directly from the checked Swan AST through Swan-owned x86-64 System V and AArch64 AAPCS64 ELF backends. The LLVM IR generator has been removed. The build and CI do not require LLVM or Clang.

The system assembler/linker is used only after Swan emits assembly. It does not perform Swan instruction selection or optimization.

## Implemented in the initial x86-64 backend

- Integer and boolean literals.
- Local variables and function parameters.
- Integer arithmetic and signed comparisons.
- Function calls with up to six arguments.
- If/else and while control flow.
- Break and continue.
- String literals and basic console logging.
- Heap integer arrays using the platform C runtime allocator.
- GNU assembler Intel syntax output.
- AArch64 GNU assembler output and AAPCS64 register calling convention.
- AArch64 object emission through `aarch64-linux-gnu-as`.

## Known limitations

- Floating-point lowering is not implemented.
- Struct member lowering is not implemented.
- More than six call arguments are rejected.
- Only the x86-64 System V ABI backend is implemented.
- AArch64, ARM, RISC-V, and WebAssembly backends do not exist yet.
- Register allocation is currently stack-based and not optimized.
- CPU-specific instruction tuning and mature Swan-owned optimization passes are not implemented.
- Platform-specific ABIs beyond the initial x86-64 System V target are not yet validated.

These limitations are deliberate and must not be hidden behind external compiler fallbacks.

## Required next tests

Add positive and negative tests for each newly implemented language construct, ABI edge cases, stack alignment, register preservation, recursion, aggregate layout, and generated-code execution. Add a dedicated test suite for each new architecture backend before advertising support.
