# Compiler Correctness Audit

This document records correctness properties and regression coverage for the current native backend.

## Correctness coverage

| Area | Failure mode | Resolution |
|---|---|---|
| Definite returns | Non-void functions could fall through without returning | Semantic analysis requires a return on every statically visible path; an `if` counts only when both branches return |
| Void values | Void-returning calls could be inferred as variable values | Variables, parameters, and struct fields reject `void`; void remains valid as a function return type |
| Struct cycles | By-value recursive struct dependencies are invalid | The assembly backend detects dependency cycles and reports a compiler diagnostic |
| Native symbol collisions | User function names could collide with runtime or platform symbols | Non-entry Swan functions use the `swan_fn_` symbol prefix; `main` remains the process entry point |
| Empty parser input | `peek()` could underflow when the parser received an empty token vector | The parser constructor guarantees a terminal EOF token |

## Regression coverage

`tests/regressions.sh` exercises invalid return and void-type cases, return branches, forward struct references, struct cycle diagnostics, and string parameter behavior.

The empty-token parser case is exercised by the header test's direct parser smoke program.

## Scope and limitations

This is a targeted correctness pass, not a proof of compiler correctness. In particular:

- Return analysis is conservative: loops are not assumed to execute, even when their condition is a compile-time true value.
- Struct declarations are type-checked, but struct value layout and member access are not implemented by the native backend.
- The initial backend supports x86-64 Linux System V ELF only.
- The language specification and behavior of registered-but-unimplemented keywords still need a dedicated conformance effort.
- The compiler has not yet been subjected to a full fuzzing campaign or sanitizer matrix.

Every future language feature must include positive tests, negative tests, diagnostics, and generated-assembly execution tests where runtime behavior is involved.