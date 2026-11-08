# Compiler Correctness Audit

## Active implementation

- C11 compiler; `main.c` is the only compiled translation unit.
- Header-only compiler modules under `compiler/include/swan/`.
- Direct Swan-owned x86-64 System V assembly generation.
- GNU assembler Intel syntax; system assembler/linker used only after code generation.
- Source-level `import` resolution for standard-library modules.
- Root license: Apache License 2.0.

## Implemented language and backend subset

- Signed 64-bit integer and boolean expressions.
- Local bindings, function parameters, functions, and direct calls with up to six arguments.
- `if` / `else`, `while`, `break`, and `continue`.
- Arithmetic, signed comparisons, `and`, `or`, `not`, and short-circuit condition lowering.
- String literals in `log` and `echo!(...)` output statements.
- Checked and unchecked contiguous `u32` arrays.
- Semantic checks for duplicate functions, unresolved names/functions, function arity, immutable assignment, loop-control placement, and duplicate imports.
- x86-64 System V stack alignment and preservation of volatile local registers around calls.
- Fixed-register local allocation with stack spills, constant folding, power-of-two division, small single-return inlining, and selected loop strength reductions.

## Known limitations

- Only x86-64 System V Linux is implemented.
- Floating-point execution, general string values, structs, member access, closures, exceptions, and aggregate ABI lowering are not implemented.
- Static type checking is incomplete; declared type annotations are not yet a full type system.
- Calls with more than six arguments are rejected.
- The register allocator is local and heuristic; it is not liveness-based and does not optimize register pressure across complex control flow.
- Bounds-check elimination is limited; use of unchecked arrays remains an explicit programmer responsibility.
- There is no target-independent IR, SSA, global data-flow optimizer, or instruction scheduler.
- AArch64, ARMv7/Thumb, RISC-V, WebAssembly, Apple, and Windows backends are not implemented.

These limitations must remain explicit. The compiler must not silently fall back to LLVM, Clang, or C transpilation.

## Required tests

Every new construct requires positive and negative tests, diagnostics, generated-assembly inspection where relevant, and native execution. Every backend requires ABI, object-format, and executable tests on an appropriate runner.
