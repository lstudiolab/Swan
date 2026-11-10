# Swan Compiler Architecture

## Implementation model

Swan is a compiled language with a C11 compiler host and a Swan-authored SIL subsystem. `main.c` is the host compiler's only compiled C translation unit; most host compiler components are headers in `compiler/include/swan/`. The SIL builder and passes live as `.swan` files in `compiler/sil/` and are compiled to a native object during the build.

Swan owns source parsing, AST construction, semantic analysis, typed IR, optimization, and its native target code generators. Swan source is not transpiled to C. SVM is the target-independent compiler engine; it is not a target CPU or an interpreter.

## Build and bootstrap

`build.sh` performs these steps:

1. Compile `main.c` as a temporary C11 bootstrap compiler with SIL bridge integration disabled.
2. Concatenate the SIL source stages, removing their module import declarations for the bootstrap compilation unit.
3. Ask the bootstrap compiler to emit assembly for the SIL runtime source.
4. Assemble that output to an object.
5. Link the object with `main.c` to produce `build/swan`.

The production executable therefore includes native code generated from Swan-authored SIL. The SIL stage is intentionally small and does not replace the C11 host frontend or general typed-IR lowering.

## Active source-lowering paths

```text
.swan source
  -> UTF-8 validation / lexer
  -> indentation-aware parser / AST
  -> C-header declarations + Swan module loading
  -> semantic analysis
  -> reachability analysis
  -> choose lowering path
       SIL bridge for eligible straight-line scalar functions
       general AST -> typed SSA IR for remaining IR-supported functions
       AST -> x86-64 compatibility codegen for features outside IR
  -> SVM verified typed-IR compilation core
       IR verifier -> target-independent optimizer -> verified optimized IR
  -> selected target backend (x86-64, AArch64, ARMv7, RISC-V, WASM, LLVM)
  -> emitter / assembler / linker
```

The SIL bridge is an optimization/lowering route for a narrow subset, not a mandatory stage for every function. It first creates a general typed-IR candidate, then substitutes SIL output for eligible functions. Unsupported SIL functions retain general IR lowering.

The x86-64 path retains AST code generation for supported constructs not yet represented in typed IR, particularly arrays, output/logging, and runtime-lazy imports. Other targets consume typed IR and therefore have a narrower source-feature set.

## Compiler components

| Component | Responsibility |
|---|---|
| `core.h` | Shared tokens, errors, text buffers, arena and profiling support |
| `lexer.h` | UTF-8 validation, indentation tokens, literals and keyword recognition |
| `ast.h` / `parser.h` | AST model and indentation-aware syntax parser, including a dedicated explicit-release statement node |
| `c_header.h` | Clang-assisted import of supported C function declarations |
| `compiler.h` | Module loading, semantic pipeline, reachability and CLI-facing orchestration helpers |
| `sema.h` | Name/scope checks, declaration checks and strict `manual var` ownership analysis |
| `sil_bridge.h` | Lower eligible scalar AST functions through Swan-authored SIL into typed IR |
| `ir.h` / `irgen.h` | Typed SSA data model, verifier support and AST-to-IR lowering |
| `optimization.h` | Target-independent typed-IR optimization passes |
| `svm.h` | Legacy interpreter and `.swvm` serialization; scheduled for removal |
| `codegen_*.h` | Swan-owned target-specific lowering |
| `codegen_llvm.h` | Optional explicit LLVM IR portability bridge |
| `emitter.h` | Assembly serialization |
| `linker.h` | Host assembler/linker process invocation |

## Typed IR contract

Typed IR is target-independent SSA with function-local value IDs, basic blocks, explicit terminators, typed instructions, and PHI incoming edges. The IR model represents scalar integer and floating-point types, booleans, pointers, arithmetic, comparisons, select, PHI, alloca/load/store, and calls.

The verifier checks structural and type invariants, including value definitions, use-before-definition, dominance, CFG targets, terminators, PHI predecessor coverage, operand types, and call signatures. A data-model operation is not necessarily supported by every source lowerer or target backend.

The optimization pipeline performs width-aware constant folding, select and CFG simplification, algebraic simplification, trivial-PHI propagation, dominance-aware CSE, conservative LICM, post-hoisting CSE, and DCE. DCE uses a value worklist to propagate liveness. Transformations are checked with IR verification.

## Runtime and memory

The intended SVM architecture emits target machine code or target modules; it does not interpret IR at runtime. The current `svm.h` runtime and `.swvm` format are legacy compatibility code and are scheduled for removal. Alloca storage is lazily initialized per frame generation rather than by scanning all instructions at every function entry.

The x86-64 compatibility backend supports the currently implemented u32 array forms and SAMC cleanup. Small fixed-size non-escaping arrays can use a stack-frame fast path. The newer `manual var` syntax opts a direct array allocation into strict local single-owner checks; it does not implement borrowing or ownership transfer. Legacy `store` syntax remains compatible and is not subject to the new strict checks.

## Backend and ABI boundaries

| Backend | Object/runtime boundary | Scope |
|---|---|---|
| x86-64 | System V Linux | Native executable/assembly; typed scalar IR and broader AST compatibility features |
| AArch64 | AAPCS64 Linux | Limited scalar IR object/assembly emission |
| ARMv7 | AAPCS32 Linux, ARM state | Limited i32/i1 IR object/assembly emission |
| RISC-V64 | Linux integer ABI subset | Limited scalar IR object/assembly emission |
| WASM32 | WebAssembly 32-bit memory | Limited scalar/control-flow module emission |
| WASM64 | WebAssembly memory64 | Limited memory64 module emission |
| SVM | Target-independent compile engine | Shared lowering, verification, and optimization before target-specific code generation |
| LLVM triple | Depends on target triple/toolchain | Explicit typed-IR portability route; Clang/LLVM must support the target |

Successful object emission is not proof of runtime ABI compatibility. Cross-target linking, full runtime support, standard-library module bundling, aggregates, and full frontend parity remain incomplete.

## Engineering invariants

1. Do not transpile Swan source to C.
2. Keep the native backend path Swan-owned.
3. Keep the LLVM bridge explicit through `--target-triple`; it is not a silent fallback.
4. Reject unsupported constructs with diagnostics rather than emitting partial code.
5. Verify typed IR before and after transformations.
6. Describe parser, semantic, IR, and backend support separately.
7. Treat performance as unproven until measured on reproducible workloads.


See [SVM Compiler Architecture](SVM_ARCHITECTURE.md) for the target-independent compiler design, target matrix, performance requirements, and migration criteria.
