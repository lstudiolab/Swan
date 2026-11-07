# Compiler Correctness Audit

## Current state

The compiler is being ported to C11 with a single `main.c` translation unit and header-only modules. The initial C backend emits x86-64 System V assembly directly. The previous C++ implementation is not used by the C build.

The system assembler/linker is used only after Swan emits assembly. It does not perform Swan instruction selection or optimization.

## Initial C backend coverage

- Integer and boolean literals.
- Local variables and function parameters.
- Integer arithmetic and signed comparisons.
- Function calls with up to six arguments.
- If/else and while control flow.
- String literals in `log` statements and integer console logging.
- Basic semantic checks for duplicate functions, unknown names/functions, call arity, and immutable bindings.
- GNU assembler Intel syntax output for x86-64 System V.

## Known limitations

- Floating-point lowering is not implemented.
- Struct member lowering is not implemented.
- Arrays, floats, structs, break/continue, full type inference/checking, and lexical shadowing are not implemented in the C port.
- Only the initial x86-64 System V C backend exists; AArch64 is not yet ported.
- ARMv7/Thumb, RISC-V, and WebAssembly backends do not exist yet.

- Register allocation is currently stack-based and not optimized.
- CPU-specific instruction tuning and mature Swan-owned optimization passes are not implemented.
- Platform-specific ABIs beyond the initial x86-64 System V target are not yet validated.

These limitations are deliberate and must not be hidden behind external compiler fallbacks.

## Required next tests

Add positive and negative tests for each newly implemented language construct, ABI edge cases, stack alignment, register preservation, recursion, aggregate layout, and generated-code execution. Add a dedicated test suite for each new architecture backend before advertising support.
