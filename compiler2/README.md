# Swan Compiler 2 — swanc

Compiler 2 is the new self-hosted compiler implementation. It is written in Swan and lives beside Compiler 1.

## Compiler ownership

- `compiler/` is **Compiler 1**, the existing C implementation. It is the Stage 0 bootstrap compiler.
- `compiler2/` is **Compiler 2**, the Swan implementation named `swanc`.
- Compiler 1 must remain intact throughout development and after Compiler 2 becomes self-hosted.
- Compiler 1 may only be deleted if the project owner explicitly instructs that deletion. Passing bootstrap tests is not permission to delete it.

## Bootstrap

1. Compiler 1 compiles the Swan sources in this directory to produce Stage 1 `swanc`.
2. Stage 1 compiles these same Swan sources to produce Stage 2 `swanc`.
3. Stage 2 compiles the same sources again. Compare compiler behavior and generated outputs, then run the complete conformance suite.
4. Keep Compiler 1 available as the recovery/bootstrap compiler. Do not remove it during cleanup or refactoring.

## Pipeline

```text
Lexer.swan
  -> Parser.swan
  -> AST.swan (50 AST validation/checker passes)
  -> Sema.swan
  -> SilGen.swan
  -> SilDemand.swan
  -> SilPasses.swan
  -> SilOptimization.swan
  -> STAIRGen.swan (SVM IR -> target assembly IR)
  -> STABackend.swan
  -> Codegen.swan
  -> Optimization.swan
  -> Linker.swan
```

The SVM remains Swan's target-independent compilation representation and optimization boundary. The STA stage lowers verified SVM IR into target-specific assembly IR; the backend and code generator emit the selected target's machine code or WebAssembly. No stage interprets SVM instructions at runtime.

## Module responsibilities

| Module | Responsibility |
|---|---|
| `Lexer.swan` | UTF-8 source scanning, tokens, spans, indentation and lexical diagnostics |
| `Parser.swan` | Grammar, precedence, declarations, expressions and statements |
| `AST.swan` | AST node model and 50 explicit structural/semantic pre-checkers |
| `Sema.swan` | Scope resolution, type checking, overload resolution, ownership and diagnostics |
| `SilGen.swan` | Lower typed AST into typed SSA SIL |
| `SilDemand.swan` | Backward value-demand and liveness analysis |
| `SilPasses.swan` | Individual SIL transformation and analysis passes |
| `SilOptimization.swan` | Ordered, verified SIL optimization pipeline |
| `STAIRGen.swan` | Lower verified SVM/SIL into target-specific assembly IR |
| `STABackend.swan` | Target ABI, instruction selection, register allocation and assembly/object emission |
| `Codegen.swan` | Coordinate target code generation and artifact emission |
| `Optimization.swan` | Whole-program and target-aware optimization orchestration |
| `Linker.swan` | Object linking, libraries, startup/runtime objects and final artifacts |

## Non-negotiable correctness rules

- Compilation is ahead-of-time. There is no bytecode interpreter or SVM execution loop.
- Unsupported or unhandled language constructs are compile errors, never silently ignored.
- Every IR transformation preserves types, control flow, ownership and source locations.
- Verify IR before and after optimization passes.
- Target-specific behavior is isolated behind the backend interfaces.
- A target is supported only when its backend has real code generation and executable tests.
- The 50 AST checkers must be individually named, independently testable, and documented. Do not count empty stubs as implemented checkers.

## Implementation status

This directory establishes Compiler 2's ownership boundary and module contracts. The files are initial scaffolds, not a claim that Compiler 2 is implemented. Existing Swan-authored SIL modules under `compiler/sil/` belong to Compiler 1's current build integration and are preserved; they can be reused or ported deliberately, but must not be moved or deleted as part of this scaffold.
