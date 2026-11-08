# Swan Compiler Pipeline Design

This document defines the intended compiler architecture. It distinguishes the target architecture from the currently implemented compiler so that no stage is mistaken for working code before it exists.

## Intended pipeline

```text
.swan source
  -> lexer.h
  -> parser.h
  -> ast.h
  -> sema.h
  -> silgen.swan
  -> sildemand.swan
  -> silpass.swan
  -> siloptimization.swan
  -> irgen.h
  -> optimization.h
  -> codegen.h
  -> emitter.h
  -> linker.h
  -> native executable
```

## Stage contracts

| Stage | Input | Output | Contract |
|---|---|---|---|
| Lexer | UTF-8 source bytes | Token stream | Preserve source locations; report lexical errors. |
| Parser | Tokens | AST | Parse syntax only; allocate AST nodes in the compilation arena. |
| AST | — | AST data model | Own node/type definitions and source spans. This is a representation, not a transformation pass. |
| Semantic analysis | AST + imported module interfaces | Validated, typed AST | Resolve names, scopes, types, overloads, effects and ownership constraints. No machine code emission. |
| SIL generation | Validated AST | High-level typed SIL | Preserve source-level control flow and semantic guarantees. |
| SIL demand | SIL | Demand/liveness facts | Compute value demand, use-def relationships, liveness and dead computations. Analysis returns facts; it must not silently mutate program meaning. |
| SIL passes | SIL + analysis facts | Transformed SIL | Run small, independently verifiable transformations. Each pass declares required and invalidated analyses. |
| SIL optimization | SIL | Optimized SIL | Coordinate canonicalization, inlining, specialization and high-level simplification. Avoid duplicating low-level machine optimizations. |
| IR generation | Optimized SIL | Typed target-independent IR | Lower to explicit control-flow blocks, typed operations, values and terminators. |
| IR optimization | IR | Optimized IR | Perform constant propagation, dead-code elimination, CFG simplification, common-subexpression elimination, loop transforms and target-independent data-flow optimization. |
| Code generation | Optimized IR + target description | Target machine representation | Instruction selection, ABI lowering, register allocation, spills, frame layout and target-specific lowering. |
| Emitter | Machine representation | Assembly or object file | Serialize selected instructions, symbols, sections, relocations and debug/source metadata. |
| Linker | Object files + runtime/library inputs | Executable or library | Resolve symbols and relocations and produce the requested artifact. |

## Important ordering rules

1. ast.h defines AST structures. It is not a pass that runs after parsing. The parser constructs those structures.
2. Import/module loading must happen before semantic analysis completes. Semantic analysis needs declarations and interfaces from imported modules. The driver may discover imports after parsing, then parse imported sources, before whole-program semantic validation.
3. SIL demand analysis runs after SIL generation and before any pass that consumes its facts. If a SIL pass changes control flow or use-def chains, affected analyses must be invalidated and recomputed.
4. silpass.swan is the pass framework/registry; siloptimization.swan is the optimization pipeline policy. Keep individual passes independently testable.
5. irgen.h lowers SIL to target-independent IR. It must not select x86-64 instructions.
6. optimization.h optimizes IR. SIL-level transformations belong in the SIL layer; target-specific peepholes belong in the backend.
7. codegen.h selects and allocates target instructions. It must not own file serialization.
8. emitter.h emits assembly/object representation. linker.h runs after all required objects and runtime components are available.
9. No stage may silently fall back to C transpilation or LLVM/Clang code generation. System assemblers/linkers may be used to assemble/link Swan-emitted output.

## Required internal data models

### SIL

SIL must have explicit value and instruction IDs; typed operands and results; basic blocks and terminators; function/module boundaries; explicit calls, branches, returns, memory and ownership operations; source locations; and a verifier for type consistency, valid operands, block termination and dominance/use-def invariants.

### Target-independent IR

IR must have explicit primitive and aggregate types; SSA values and basic blocks; an explicit control-flow graph and terminators; explicit load/store/call operations and memory effects; target-independent integer semantics and overflow behavior; a verifier; and a deterministic textual dump format.

### Backend interfaces

Each backend must provide target data layout and ABI, instruction selection, register classes and allocation, spill/reload and stack-frame layout, calling convention and symbol rules, assembly/object emission integration, and target-specific correctness and ABI tests.

## Migration sequence

1. Extract AST definitions from the parser/core implementation into ast.h without changing parsing behavior.
2. Define SIL data structures, builder API, verifier and deterministic dump.
3. Implement AST-to-SIL generation for the currently supported language subset.
4. Add SIL demand/liveness analysis and a pass manager with invalidation rules.
5. Port existing safe AST/codegen optimizations into verified SIL passes; compare generated behavior against the current backend.
6. Define typed SSA IR and its verifier.
7. Implement SIL-to-IR lowering and IR optimization passes.
8. Implement an x86-64 backend consuming IR. Keep the current AST backend behind a temporary comparison switch until differential tests pass.
9. Separate machine instruction selection from assembly emission.
10. Move assembler/linker invocation into the linker driver and add object-file support.
11. Only then advertise additional targets, each backed by ABI and execution tests.

## Correctness gates

- Every pass has unit tests and a verifier run before/after transformation in debug builds.
- A corpus of existing .swan programs must produce equivalent observable results before and after migration.
- Differential tests compare old and new backends during transition.
- Generated assembly is inspected for ABI correctness, stack alignment and register preservation.
- Performance comparisons use identical workloads, validated outputs and repeated runs. Never trade correctness for a benchmark result.

## Current status

The production native path remains the established C11 lexer/parser/semantic checker and direct AST-to-x86-64 code generator. Existing `.swan` compiler and standard-library tests continue to exercise that path.

The following components now exist and are exercised independently:

- `ast.h` owns AST structures and arena lifetime helpers; `parser.h` constructs those nodes.
- `ir.h` defines typed SSA IR and a structural verifier.
- `irgen.h` lowers a conservative scalar subset (integer/boolean literals, parameters, immutable scalar locals, unary operations, arithmetic/comparisons and a single explicit return) from AST to IR. Unsupported constructs fail explicitly.
- `optimization.h` runs verified target-independent scalar constant folding.
- `svm.h` implements a portable register-based IR interpreter with fuel limits, call-depth checks, arithmetic traps, branches and PHI handling. Memory instructions are not implemented. The versioned little-endian module header exists, but a complete standalone module serializer/loader does not.
- `codegen.h` is the target dispatch boundary; x86-64 remains the only native machine-code backend.
- `emitter.h` and `linker.h` expose separated assembly-output and host-linking helpers. The legacy driver still uses parts of its inline output/linking path.
- `main.c` now owns CLI/compiler orchestration; `compiler.h` supplies reusable compiler support functions.

The `.swan -> AST -> typed IR -> optimization -> SVM execution` path is covered by `tests/irgen.sh`. The native `.swan` compiler suite, IR verifier, SVM runtime tests and IR-generation integration tests run in CI.

Still not implemented: complete lowering for the language, mutable-variable/control-flow lowering in IRGen, memory and full call ABI support in SVM, a standalone SVM binary serializer/loader, a machine-code JIT in SVM, a fully integrated IR-first production pipeline, SIL/demand/pass infrastructure, and native AArch64, ARMv7, RISC-V64, WASM32 and WASM64 backends. These targets must remain unavailable until their code generators and ABI/execution tests exist. This is a tested foundation, not universal native binary support.
