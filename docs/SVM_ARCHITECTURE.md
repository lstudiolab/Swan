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

## Native x86-64 machine-code lowering

For source programs accepted by the typed-IR frontend, the x86-64 SVM backend emits verified LLVM IR and invokes Clang at `-O3 -march=x86-64 -mtune=generic`. LLVM performs target instruction selection, register allocation, machine scheduling, and assembly emission. The driver then assembles and links that native assembly. This is ahead-of-time compilation: the generated program contains no SVM interpreter or IR dispatch loop.

The AST compatibility backend remains for implemented source constructs not yet represented in typed IR. It also emits native x86-64 assembly; it is not an interpreter, but it does not receive LLVM's full machine-level optimization pipeline. The supported IR subset should expand so more programs use the optimizing native path. LLVM/Clang is therefore required for the optimized typed-IR x86-64 path; missing Clang produces a diagnostic rather than an interpreter fallback.

## Single-file portable distribution

The `--target portable` alias emits one WebAssembly 32-bit module (`.wasm`) from the complete Swan compilation unit. WebAssembly is architecture-neutral and OS-neutral at the module level; a compatible WebAssembly runtime is required on each host. This is the portable single-file distribution format and does not interpret Swan source or SVM IR. Native executables remain target-OS/ABI-specific. The native x86-64 backend uses a generic x86-64 ISA baseline rather than `-march=native`, so it does not accidentally emit instructions that only exist on the build machine's CPU.

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

The repository has target-specific IR emitters for x86-64, AArch64, ARMv7, RISC-V64, WebAssembly, and an optional LLVM bridge. `swan_svm_prepare_ir` is the shared lowering, optimization, and verification entry point, and `swan_svm_codegen` dispatches the verified module to x86-64, AArch64, ARMv7, RISC-V64, WASM32/64, or LLVM. The legacy IR interpreter, `.swvm` serializer, `swan vm` command, and `--target svm` target have been removed. Backend feature coverage is still unequal and the driver retains target-specific object/link handling; the next migration step is a data-driven backend registry and common object/link pipeline.

The shared SVM frontend, optimizer, verifier, and backend dispatcher are now used for all registered compiled targets. Completion still requires consolidating target-specific object/link orchestration, expanding each backend to the same language subset, and passing per-target compile/link/run or module-validation tests.
