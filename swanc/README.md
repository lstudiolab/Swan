# swanc — Swan Compiler 2

Compiler 2 is Swan's self-hosted compiler. It follows the **architectural separation used by the Swift compiler**: parsing and semantic analysis produce a typed AST; SILGen lowers that AST into a high-level, ownership-aware SIL; SIL analyses and optimizations operate before IRGen; IRGen lowers SIL into Swan's target-independent machine IR; SVM generation and optimization prepare it for target lowering, code generation, and linking.

This is an architectural model, not a dependency on the Swift compiler or LLVM. Swan owns its language semantics, SIL, IR, SVM, optimizers, and backends.

Compiler 1 remains in `compiler/` as the bootstrap compiler and behavioral reference until Compiler 2 can compile itself and pass the same regression suite.

## Compiler pipeline

```text
.swan source
    |
    v
 driver/driver.swan
    |
    v
 lexer/Lexer.swan
    |
    v
 parser/Parser.swan
    |
    v
 ast/AST.swan
    |
    v
 sema/sema.swan
    |
    v
 sil/Silgen.swan
    |
    v
 SIL verification
    |
    v
 sil/Sildemand.swan
    |
    v
 sil/SilOptimization.swan
    |
    v
 ir/IRGen.swan
    |
    v
 SVM IR verification
    |
    v
 svm/SvmGen.swan
    |
    v
 svm/SvmOptimization.swan
    |
    v
 backend/targetbackend.swan
    |
    v
 backend/codegen.swan
    |
    v
 linker/linker.swan
    |
    v
 executable / object / assembly / WebAssembly
```

## Responsibilities and boundaries

- **Driver:** command-line interface, build modes, target selection, source loading, diagnostics, output paths, and orchestration only.
- **Lexer:** UTF-8 scanning and tokenization. It remains one cohesive file.
- **Parser:** source grammar and AST construction. It remains one cohesive file.
- **AST:** source-level syntax representation, traversal, and structural invariants.
- **Sema:** module/name resolution, type checking, inference, control-flow validation, definite initialization, and ownership/borrowing checks.
- **SILGen:** AST-to-SIL lowering. SIL is typed, SSA-based, control-flow explicit, and ownership-aware.
- **SIL demand analysis:** value liveness, use/def relationships, and observable-effect tracking used by SIL passes.
- **SIL optimization:** high-level transformations while source-language type and ownership information remains available.
- **IRGen:** lower verified SIL into Swan's lower-level, target-independent IR. This is Swan's analogue of Swift IRGen; it does not emit LLVM IR.
- **SVM generation:** lower typed IR into SVM's machine-level representation. SVM is a compiler representation, not an interpreter or CPU target.
- **SVM optimization:** machine-level constant propagation, DCE, CSE, CFG simplification, loop transforms, and other verified passes.
- **Target backend:** target triple, ABI, data layout, instruction legalization, and target lowering.
- **Codegen:** emit assembly, object files, or WebAssembly from target-lowered instructions.
- **Linker:** invoke the correct target toolchain and produce the final artifact.

A stage may only consume the output contract of the preceding stage. No stage may silently skip verification. Unsupported syntax, semantic constructs, IR operations, or target instructions are hard compilation errors.

## Source layout

```text
swanc/
├── driver/
│   └── driver.swan
├── lexer/
│   └── Lexer.swan
├── parser/
│   └── Parser.swan
├── ast/
│   └── AST.swan
├── sema/
│   └── sema.swan
├── sil/
│   ├── Silgen.swan
│   ├── Sildemand.swan
│   └── SilOptimization.swan
├── ir/
│   ├── IR.swan
│   └── IRGen.swan
├── svm/
│   ├── SvmGen.swan
│   └── SvmOptimization.swan
├── backend/
│   ├── targetbackend.swan
│   └── codegen.swan
├── linker/
│   └── linker.swan
└── optimization/
    └── ConstantFold.swan
```

Keep the lexer and parser as single cohesive source files. Other subsystems should be decomposed into focused files as their implementations grow. Do not create a monolithic compiler file.

## Implementation status

The files added for the Swift-inspired stages currently define architectural contracts and invariants; they are **not yet complete implementations** and are not wired into an executable driver.

Existing implemented slices:
- Lexer: scans UTF-8 source bytes into caller-owned token storage. Token records use five words: kind, byte offset, byte length, starting line, and starting column.
- Parser: currently parses integer expressions using Pratt precedence. It is not yet a complete source-file parser.
- AST: initial five-word expression-node representation and structural validation.
- IR: initial eight-word linear typed instruction representation, SSA result numbering, and structural/use-before-definition verification.
- Optimization: narrow i32 constant folding for add, subtract, and multiply.

The previous C implementation remains the behavioral reference for language semantics, diagnostics, SIL bridge behavior, IR invariants, optimizations, target ABIs, code generation, linking, and regression tests. Port and verify each stage before enabling it in the driver.

## Bootstrap contract

1. Compiler 1 builds Compiler 2 from the Swan sources in `swanc/`.
2. Compiler 2 compiles its complete source tree without invoking C implementations for compiler stages.
3. The stage-2 compiler rebuilds the same source tree.
4. Stage-2 and stage-3 outputs are deterministic, or metadata is normalized before comparison.
5. Both stages pass the same positive, negative, regression, and target tests.
6. Keep Compiler 1 until bootstrap is repeatable and verified.

A successful build of a partial Compiler 2 is not proof of self-hosting.
