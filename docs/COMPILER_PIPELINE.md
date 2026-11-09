# Swan compiler pipeline

The compiler keeps a C11 host for command-line orchestration, lexing, parsing,
AST construction, and semantic analysis. The SIL implementation itself is
written in Swan and compiled into the final compiler during bootstrap.

## Ordered pipeline

```text
.swan source
  -> lexer.h
  -> parser.h
  -> ast.h
  -> import resolution
  -> sema.h
  -> silgen.swan
  -> sildemand.swan
  -> silpass.swan
  -> siloptimization.swan
  -> irgen.h (SIL -> typed SSA IR bridge)
  -> optimization.h
  -> target-specific codegen
  -> emitter.h
  -> linker.h / native linker
  -> executable or target artifact
```

Imports are resolved before semantic analysis completes. SIL is verified before
and after optimization. Typed target-independent IR is verified and optimized
before target code generation.

## Swan-authored SIL and bootstrap

The build first creates a temporary C11 bootstrap compiler. It concatenates the
Swan stage sources (removing module import declarations for this single
compilation unit), asks the bootstrap compiler to emit assembly, assembles that
output, and links the resulting object into the final `build/swan`.

The final compiler calls exported functions generated from Swan source:
`sil_emit_param`, `sil_emit_const`, `sil_emit_binary`,
`sil_emit_return`, `sil_verify`, `sil_demand`,
`sil_fold_constants`, and `sil_optimize`. The C bridge translates supported
AST expressions into SIL and lowers optimized SIL into the existing typed SSA
IR. SIL passes are not duplicated as C implementations.

## Initial SIL instruction model

Each instruction is five 32-bit words:

| Word | Meaning |
|---|---|
| 0 | Encoded opcode and result type |
| 1 | SSA destination |
| 2 | Left operand |
| 3 | Right operand |
| 4 | Immediate or parameter index |

Types are `i32`, `i64`, and `bool`. The implemented scalar operations are
parameters, constants, add, subtract, multiply, equality, signed-less-than,
copy, and return. The Swan-authored pass sequence performs structural/type
verification, constant propagation/folding, backwards value demand, dead
instruction elimination, and verification again.

## Compatibility and migration boundary

The C AST-to-SIL bridge currently accepts straight-line scalar functions whose
body is a single return expression. It handles scalar parameters, literals,
addition, subtraction, multiplication, equality, and less-than. Unsupported
constructs continue through the established AST-to-IR generator or AST native
backend; they are not rejected or compiled with incomplete semantics.

This fallback is migration scaffolding. The next SIL expansion is control-flow
blocks and branch terminators, followed by variables/memory and function calls.
Loops, imports, and general function bodies move to SIL only after verifier and
end-to-end coverage exists for those constructs.

## Backend constraints

Swan keeps its own typed SSA IR optimizations and target backends. This pipeline
does not introduce LLVM. The existing x86-64, AArch64, ARMv7, RISC-V64,
WebAssembly, and SVM paths remain Swan-owned.
