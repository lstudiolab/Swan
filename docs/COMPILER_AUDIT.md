# Compiler Correctness Audit

## Current C11 implementation

The active compiler is C11. \`main.c\` is the only compiled source file; all compiler subsystems are implemented as headers under \`compiler/include/swan/\`. The previous C++ compiler files have been removed from the repository.

Swan emits x86-64 System V assembly directly. The host assembler and linker only assemble and link the emitted assembly.

## Implemented subset

- Integer and boolean literals.
- Local variables and function parameters.
- Integer arithmetic and signed comparisons.
- Function calls with up to six arguments.
- \`if\`/\`else\` and \`while\` control flow.
- String literals in \`log\` statements.
- Integer and string console logging.
- Semantic checks for duplicate functions, unresolved names/functions, function call arity, and assignment to immutable bindings.
- GNU assembler Intel syntax output for x86-64 System V.

## Known limitations

- Arrays, floating point, structs, member access, break/continue, and general string values are not implemented in the C port.
- Type inference and complete static type checking are not implemented.
- Lexical shadowing and complete scope rules are not implemented.
- More than six call arguments are rejected.
- Nested calls inside expressions require further ABI/stack-alignment validation.
- Only x86-64 System V on Linux is implemented in the C port.
- AArch64, ARMv7/Thumb, RISC-V, and WebAssembly backends are not ported yet.
- Register allocation and mature optimization passes are not implemented.

These gaps must remain explicit. The compiler must never silently fall back to LLVM, Clang, or a C transpilation stage.

## Required next tests

Add positive and negative tests for every newly ported language construct, recursion, nested calls, ABI stack alignment, register preservation, aggregate layout, memory operations, and generated-code execution. Each architecture backend requires its own ABI and execution test suite before it is advertised as supported.
