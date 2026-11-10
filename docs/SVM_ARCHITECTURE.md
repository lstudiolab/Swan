# SVM Compiler Architecture

## Decision

SVM means **Swan Virtual Machine compiler engine**, not an interpreter and not a target CPU. It is the target-independent compilation core that takes source after lexical analysis and drives it through verified, optimized intermediate representations into the selected target backend.

SVM must never execute Swan source or typed IR by walking instructions in a runtime dispatch loop. The old register-frame interpreter and serialized-IR `.swvm` artifact are legacy behavior and are not the architecture to extend.

## One pipeline, many targets

```text
.swan source
    |
    v
Lexer -> tokens
    |
    v
Parser -> AST
    |
    v
Semantic analysis + type checking
    |
    v
SVM frontend lowering
    |  SIL bridge / general AST-to-typed SSA IR
    v
Typed IR verifier
    |
    v
SVM optimizer
    |  constant folding, CFG simplification, CSE, LICM, DCE, ...
    v
Verified optimized target-independent IR
    |
    +--> x86-64 instruction selection + register allocation + assembly
    |
    +--> AArch64 instruction selection + register allocation + assembly
    |
    +--> ARM/ARM64 instruction selection + register allocation + assembly
    |
    +--> RISC-V instruction selection + register allocation + assembly
    |
    +--> WebAssembly lowering + module emission
    |
    +--> LLVM IR bridge for additional triples and architectures
    |
    v
Assembler / object writer / linker -> native executable or target module
```

Every backend consumes the same verified, optimized IR contract. A target backend may reject an operation it cannot lower, but it must produce a diagnostic; it must not fall back to interpreting IR or silently emit placeholder behavior.

## Responsibilities

- **Frontend:** UTF-8 validation, lexing, parsing, imports, name resolution, type checking, and diagnostics.
- **SVM lowering:** converts the typed source representation into target-independent typed SSA IR. SIL is an optional frontend lowering path, not a target backend.
- **SVM verifier:** validates types, SSA definitions, control-flow edges, PHI inputs, calls, and terminators before and after transformations.
- **SVM optimizer:** performs target-independent optimizations and dead-code elimination. Backend-specific optimization follows instruction selection.
- **Target backends:** perform target-specific legalization, instruction selection, register allocation, frame layout, ABI lowering, assembly or binary emission.
- **Toolchain driver:** invokes assemblers and linkers for the selected object format and target ABI.

## Target matrix

The architecture is target-independent. x86-64 is one backend, not the default internal representation and not a prerequisite for other targets.

| Target family | Backend output |
|---|---|
| x86-64 | Native machine assembly and object code |
| AArch64 / ARM64 | Native machine assembly and object code |
| ARMv7 / ARM32 | Native machine assembly and object code |
| RISC-V 32/64 | Native machine assembly and object code |
| WebAssembly 32/64 | Wasm module |
| Other LLVM-supported targets | LLVM IR followed by the selected LLVM/Clang target backend |

Each target has its own instruction selector, register model, calling convention, data layout, and object format. Shared IR does not mean shared machine instructions or shared ABI assumptions.

## Performance requirements

1. No interpreter dispatch loop in compiled execution.
2. No IR instruction-by-instruction runtime execution for normal `run` or `build`.
3. Perform optimization before target lowering; run target-specific peephole and machine-level passes after lowering.
4. Use liveness analysis and register allocation to minimize spills and memory traffic.
5. Respect each target's ABI, alignment, stack rules, register classes, and relocation model.
6. Keep compile-time profiling separate from generated-program runtime benchmarks.
7. Benchmark every target with correctness checks and reproducible workloads before making speed claims.

## Strictness

Unsupported source constructs must fail during semantic analysis or lowering. Unsupported IR operations must fail in the verifier or target legalizer. Unsupported target features must fail in the backend with the target and operation identified. No silent fallback to the old interpreter is allowed.

## Migration status

The repository has target-specific IR emitters for x86-64, AArch64, ARMv7, RISC-V64, WebAssembly, and an optional LLVM bridge. `swan_svm_prepare_ir` is now the shared lowering, optimization, and verification entry point used by every target pipeline. The legacy IR interpreter, `.swvm` serializer, `swan vm` command, and `--target svm` target have been removed. Backend feature coverage is still unequal and the driver still has target-specific output handling; the next migration step is a unified backend registry and common object/link pipeline.

The shared SVM frontend/optimization pipeline is now used for all registered compiled targets. Completion still requires consolidating target-specific emission and linker orchestration, expanding each backend to the same language subset, and passing per-target compile/link/run or module-validation tests.
