# Compiler Correctness Audit

This document records correctness properties and regression coverage for Swan's native LLVM IR backend.

## Correctness coverage

| Area | Failure mode | Resolution |
|---|---|---|
| Definite returns | Non-void functions could fall through without returning | Semantic analysis requires a return on every statically visible path; an `if` counts only when both branches return |
| Void values | Void-returning calls could be inferred as variable values | Variables, parameters, and struct fields reject `void`; void remains valid as a function return type |
| Struct cycles | By-value recursive struct dependencies are invalid | The native IR backend detects dependency cycles and reports a compiler diagnostic |
| Native symbol collisions | User function names could collide with runtime or platform symbols | Non-entry Swan functions use the `swan_fn_` symbol prefix; `main` remains the process entry point |
| Empty parser input | `peek()` could underflow when the parser received an empty token vector | The parser constructor guarantees a terminal EOF token |
| Target ABI width | 32-bit targets use 32-bit `size_t` for allocation calls | The IR lowering selects the allocation size type from the requested target triple |

## Backend coverage

The checked Swan AST lowers directly to LLVM IR. The LLVM target backend emits assembly for the targets exercised by CI:

- x86-64
- AArch64
- ARMv7
- RISC-V 64
- WebAssembly 32
- Apple iOS/macOS, Android, and Windows target triples

The installed LLVM build determines the complete set of available target triples. Cross-target linking additionally requires the platform linker, runtime, and sysroot.

## Regression coverage

`tests/regressions.sh` exercises invalid return and void-type cases, return branches, forward struct references, struct cycle diagnostics, and string parameter behavior.

`tests/architecture.sh` generates and validates assembly for the supported architecture matrix and runs a native array/function workload.

## Scope and limitations

This is a targeted correctness pass, not a proof of compiler correctness. In particular:

- Return analysis is conservative: loops are not assumed to execute, even when their condition is a compile-time true value.
- Struct declarations and cycle diagnostics are supported; member access is still rejected by semantic analysis.
- The current typed AST-to-LLVM IR lowering is an initial implementation and still needs complete language-feature conformance.
- The compiler has not yet been subjected to a full fuzzing campaign or sanitizer matrix.

Every future language feature must include positive tests, negative tests, diagnostics, and generated-code execution tests where runtime behavior is involved.
