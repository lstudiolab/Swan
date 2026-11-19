<p align="center"><img src="../assets/brew-logo.png" alt="Brew" width="180"></p>

# Brew Compiler Implementation Audit

This document records the implementation boundary visible in the production source tree. Syntax, semantic checking, lowering, and target support are separate milestones.

## Production compiler

- Compiler 1, implemented in C11, is the only compiler.
- `build.sh` compiles the production compiler directly. It does not bootstrap or generate another compiler.
- The root `main.c` remains a compatibility include of `compiler/tools/brewc/main.c`.
- Compiler subsystems are organized under `compiler/lib/`, with stable compatibility headers under `compiler/include/brew/`.

## Confirmed architecture

- UTF-8-aware, indentation-sensitive lexer and parser.
- AST semantic analysis, standard-library loading, C-header declaration import, and function reachability.
- Typed, target-independent SSA IR with checked storage growth, overflow guards, SSA/dominance verification, operation-specific operand contracts, integer-only arithmetic typing, and leading-PHI validation.
- Direct AST-to-typed-IR lowering for the supported scalar/control-flow subset.
- Typed-IR optimizer with constant folding, CFG simplification, algebraic simplification, PHI propagation, CSE, LICM, and worklist DCE.
- Shared BVM lowering/verification/optimization core feeding Brew-owned x86-64, AArch64, ARMv7, RISC-V64, WASM32, and WASM64 backends.
- Optional, explicit LLVM IR bridge for target triples supported by the installed toolchain.
- x86-64 AST compatibility backend for supported features not yet covered by typed IR.
- Stage profiling for frontend, IR, optimization, target code generation, assembly, linking, and runtime, plus per-pass timing and optimization pass counts.

The typed-IR verifier rejects malformed function/block storage, invalid operation/result combinations, pointer constants, non-integer arithmetic, misplaced PHIs, invalid call signatures, unused operands on fixed-arity operations, and malformed terminator fields. Dedicated negative tests cover these boundaries; builder APIs also reject null inputs and allocation-size overflow before mutating the IR.

## Source lowering

The general typed-IR generator currently covers the implemented scalar/control-flow subset: integer and boolean expressions, direct calls, locals, assignment, if/else, while, unconditional `loop:`, break/continue, pass, and short-circuit AND/OR. `loop:` lowers to the same verified CFG form as `while` and is exercised across registered native/WASM backends.

The x86-64 AST compatibility path retains features that are not yet lowered through typed IR, notably the supported u32 array forms, output/logging, and runtime-lazy imported modules. This route is selected by source-feature requirements, not as recovery from an IR or optimizer failure.

## Memory and ownership

- Eligible local arrays use compiler-generated cleanup without runtime reference counting. Small fixed-size, non-escaping arrays use native stack-frame storage.
- The x86-64 AST compatibility backend null-initializes eligible heap-array handles before conditional control flow, null-checks and clears handles during cleanup, and releases non-escaping arrays after their final top-level use when the AST liveness walk proves no later statement references them.
- Branches and loops are treated conservatively as enclosing statements for last-use analysis. Early returns and the function epilogue can both emit cleanup safely because released handles are cleared.
- Escaping arrays remain excluded from last-use cleanup; ownership transfer and explicit borrow semantics are not yet implemented. Typed-IR backends do not yet share this array lifetime implementation.
- Manual allocation modifiers and the source-level `free(...)` intrinsic have been removed from the parser and semantic checker. `free` remains an ordinary identifier; generated native cleanup may still call the platform allocator's `free` function.

## Backend status

| Backend | Verified implementation boundary |
|---|---|
| x86-64 System V Linux | Native executable/assembly; scalar typed-IR path and AST compatibility path for supported arrays, output, and lazy imports |
| AArch64 Linux | Brew-owned object/assembly for a limited scalar typed-IR subset |
| ARMv7 Linux, ARM state | Brew-owned object/assembly for a limited i32/i1 subset |
| RISC-V64 Linux | Brew-owned object/assembly for a limited scalar typed-IR subset |
| WASM32 | Brew-owned WebAssembly module for the implemented scalar/control-flow subset |
| WASM64 | Memory64 module emission for its implemented subset; runtime interoperability must be tested with a compatible engine |
| Explicit LLVM triple | LLVM IR, assembly, or relocatable object for the subset lowerable to Brew IR and supported by the installed toolchain |

BVM is the target-independent compiler engine, not a runtime target. The old IR interpreter and `.bvm` artifact have been removed. Cross-target object-format checks establish emission, not execution on the target CPU. The typed-IR backend subset is narrower than the full x86-64 compatibility path. C header imports and native external calls currently target x86-64 System V; the importer supports scalar integer/bool declarations, not general C types.

## Important limitations

- Struct/aggregate layout and ABI lowering are incomplete.
- Dotted paths parse, but full struct-instance field and method resolution is not complete.
- Floating-point source execution is not complete even though IR types and some portability emission paths represent float types.
- Arrays, slices, strings/output, and runtime-lazy imports are not yet generally represented in typed IR.
- Closures, full pointer/reference semantics, borrowing, ownership transfer, destructors, exception unwinding, and general heap objects are not implemented. Automatic last-use cleanup currently covers only eligible local arrays in the x86-64 AST compatibility backend; it is not yet a general ownership system.
- The frontend keyword registry contains syntax vocabulary beyond the implemented language subset. A recognized keyword does not imply complete parsing, semantic analysis, lowering, or runtime behavior.
- Full cross-target runtime linking and OS/platform runtime integration are incomplete.
- The IR machine-code backends are not yet equivalent to a globally register-allocated, scheduled optimizing backend.

## Test policy

The top-level `test.sh` builds the single compiler and runs direct-build, header-isolation, C11 smoke, C ABI, UTF-8, profiling, smoke, regression, Foundation, lazy-import, architecture, and keyword tests.

For each new language feature, add positive parser/semantic tests, negative diagnostic tests, IR/verifier tests where applicable, backend tests for every target claiming support, and runtime tests where the target can be executed. Add benchmarks when making performance claims.

Do not infer complete target support from a successful compile or object emission alone.
