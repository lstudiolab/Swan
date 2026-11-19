<p align="center"><img src="../assets/brew-logo.png" alt="Brew" width="180"></p>

# Brew Compiler Architecture

## Production compiler

Brew has one production compiler: Compiler 1, implemented in C11. The sole compiled compiler translation unit is `compiler/tools/brewc/main.c`; focused subsystem headers live under `compiler/lib/` and stable compatibility headers live under `compiler/include/brew/`.

`build.sh` compiles the production executable directly to `build/brewc`. `build/brew` is a compatibility alias to that same executable. The build does not create a temporary bootstrap compiler, compile a second compiler, concatenate Brew compiler sources, or link generated compiler objects.

## Compiler pipeline

```text
Brew source
  -> source manager and UTF-8 validation
  -> lexer
  -> indentation-aware parser and AST
  -> imports and shared project/module graph
  -> semantic analysis and reachability
  -> AST -> typed BVM IR
  -> IR verification
  -> target-independent optimization
  -> final IR verification
  -> selected Brew target backend (x86-64, AArch64/ARM64, or WASM64)
  -> assembly/object/WebAssembly emission
  -> platform assembler and linker where applicable
```

The x86-64 AST compatibility backend remains for supported language features not yet represented in typed IR. The driver selects it based on source features before attempting IR lowering. It is not a fallback for a failed IR pipeline.

## Compiler components

| Component | Responsibility |
|---|---|
| `Basic/Core.h` | Shared tokens, errors, text buffers, arena and profiling support |
| `Parse/Lexer.h` | UTF-8 validation, indentation tokens, literals and keyword recognition |
| `AST/AST.h` / `Parse/Parser.h` | AST model and indentation-aware syntax parser |
| `Sema/Sema.h` | Name/scope checks, declarations and semantic validation |
| `Sema/CHeader.h` | Clang-assisted import of supported C function declarations |
| `Driver/Compiler.h` | Module loading, source ownership, semantic pipeline and CLI orchestration helpers |
| `IR/IR.h` / `IR/IRGen.h` | Typed SSA data model, verifier and AST-to-IR lowering |
| `IR/Optimization.h` | Target-independent typed-IR optimization passes and pass manager |
| `CodeGen/CodeGen.h` | Shared BVM compile engine and target dispatcher |
| `CodeGen/Targets/` | Brew-owned target-specific lowering and emission |
| `Basic/Linker.h` / `Serialization/AssemblyEmitter.h` | Platform linker invocation and assembly serialization |

## Typed Ir

Typed IR is target-independent SSA with function-local value IDs, basic blocks, explicit terminators, typed instructions, and PHI incoming edges. The IR model represents scalar integer and floating-point types, booleans, pointers, arithmetic, comparisons, select, PHI, alloca/load/store, and calls.

The verifier checks structural and type invariants, including value definitions, use-before-definition, dominance, CFG targets, terminators, PHI predecessor coverage, operand types, and call signatures. A data-model operation is not necessarily supported by every source lowerer or target backend.

The optimizer performs width-aware constant folding, select and CFG simplification, algebraic simplification, trivial-PHI propagation, dominance-aware CSE, conservative LICM, post-hoisting CSE, and worklist DCE. The fixed-point pass manager is bounded to 350 pass executions and records per-pass timing when profiling is enabled.

## Runtime and memory

BVM emits target machine code or target modules; it does not interpret IR at runtime. The legacy IR interpreter, `.bvm` serializer, `brew vm` command, and `--target bvm` target have been removed. Normal execution compiles to the selected native target; WASM64 emits a memory64 WebAssembly module.

The x86-64 compatibility backend supports the currently implemented u32 array forms and automatic cleanup for eligible local arrays. Small fixed-size non-escaping arrays can use a stack-frame fast path. This is not yet a general ownership or borrow-checking system.

## Backend boundaries

| Backend | Object/runtime boundary | Scope |
|---|---|---|
| x86-64 | System V Linux | Native executable/assembly; typed scalar IR and broader AST compatibility features |
| AArch64 / ARM64 | AAPCS64 Linux | Limited scalar IR object/assembly emission |
| WASM64 | WebAssembly memory64 | Limited memory64 module emission |

Successful object emission is not proof of runtime ABI compatibility. Cross-target linking, full runtime support, standard-library module bundling, aggregates, and full frontend parity remain incomplete.

## Engineering invariants

1. Do not transpile Brew source to C.
2. Keep the native backend path Brew-owned.
3. Expose only the x86-64, AArch64/ARM64, and WASM64 target backends.
4. Reject unsupported constructs with diagnostics rather than emitting partial code.
5. Verify typed IR before and after transformations.
6. Describe parser, semantic, IR, and backend support separately.
7. Treat performance as unproven until measured on reproducible workloads.

See [BVM Compiler Architecture](bvm_architecture.md) for the target-independent compiler design and performance requirements.


## Building every supported target

Use `brewc build <source.brew> --target all -o <directory>` to compile one source project for all registered backends. The compiler parses and semantically checks the source once, then emits a native x86-64 executable, an ARM64 ELF object, and a WASM64 memory64 module. The ARM64 object still requires target linking, and the WebAssembly module requires a memory64 runtime. This mode currently rejects C ABI inputs, custom linker arguments, runtime-lazy imports, and `--no-heap`; source constructs must be supported by each backend.
