# Swan compiler pipeline

The compiler keeps a C11 host for command-line orchestration, lexing, parsing,
AST construction, and semantic analysis. The SIL implementation itself is
written in Swan and compiled into the final compiler during bootstrap.

## Ordered pipeline

```text
.swan source
  -> lexer.h
  -> parser.h / AST
  -> import resolution
  -> sema.h
  -> silgen.swan
  -> silverify.swan
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

Each instruction is five 32-bit words: encoded opcode/type, destination, left
operand, right operand, and immediate/parameter index. Types are `i32`,
`i64`, and `bool`. The current scalar operations include parameters,
constants, add/subtract/multiply, equality, signed-less-than, copy, and return.
The Swan-authored passes perform structural/type verification, constant
propagation/folding, backwards value demand, dead-instruction elimination,
and post-pass verification.

## Compatibility boundary

The AST-to-SIL bridge currently accepts straight-line scalar functions whose
body is a single return expression. It handles scalar parameters, literals,
addition, subtraction, multiplication, equality, and less-than. Unsupported
constructs continue through the existing typed-IR or AST compatibility paths.
The next SIL expansion is CFG blocks and branch terminators, then variables,
memory operations, calls, loops, and imports. The compatibility paths are not
removed until the equivalent SIL lowering and verifier coverage exists.

## Backend constraints

Swan keeps its own typed SSA IR optimizations and target backends. This pipeline
does not introduce LLVM. The existing x86-64, AArch64, ARMv7, RISC-V64,
WebAssembly, and SVM paths remain Swan-owned.
