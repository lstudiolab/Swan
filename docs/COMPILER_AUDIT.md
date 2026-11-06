# Compiler Correctness Audit

This document records the static audit findings and the regression coverage added to address them.

## Correctness fixes

| Area | Failure mode | Resolution |
|---|---|---|
| Definite returns | Non-void functions could fall through without returning | Semantic analysis now requires a return on every statically visible path; an `if` counts only when both branches return |
| Void values | Void-returning calls could be inferred as variable values | Variables, parameters, and struct fields reject `void`; void remains valid as a function return type |
| Struct ordering | A struct could be emitted before a by-value field type it depends on | C generation emits forward typedefs and topologically orders complete struct definitions |
| Struct cycles | By-value recursive struct dependencies produce invalid C | The generator detects dependency cycles and reports a compiler diagnostic |
| C identifier collisions | Swan names could collide with C keywords, library names, or separate declaration namespaces | Generated C uses separate prefixes for Swan functions, variables, fields, and types; `main` remains the C entry point |
| Empty parser input | `peek()` could underflow when the parser received an empty token vector | The parser constructor now guarantees a terminal EOF token |

## Regression coverage

`tests/regressions.sh` exercises the failures above, including positive tests for complete return branches, forward struct dependencies, and a local variable sharing a name with a function.

The empty-token parser case is exercised by the header test's direct parser smoke program.

## Scope and limitations

This is a targeted correctness pass, not a proof of compiler correctness. In particular:

- Return analysis is conservative: loops are not assumed to execute, even when their condition is a compile-time true value.
- The AST does not yet preserve semantic type results, so code generation still has some type-inference work of its own.
- Member access remains explicitly unsupported by semantic analysis.
- The language specification and behavior of registered-but-unimplemented keywords still need a dedicated conformance effort.
- The compiler has not yet been subjected to a full fuzzing campaign or sanitizer matrix.

Every future language feature must include positive tests, negative tests, diagnostics, and generated-C validation where applicable.
