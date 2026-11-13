# Swan Compiler Implementation Audit

This document records the implementation boundary visible in the current source tree. It distinguishes language syntax, semantic checking, lowering, and target support; those are separate milestones.

## Language keyword contract

The lexer contract is exactly 31 reserved words: `func`, `struct`, `let`, `var`, `null`, `if`, `else`, `const`, `type`, `match`, `guard`, `block`, `while`, `loop`, `true`, `false`, `break`, `continue`, `return`, `as`, `from`, `self`, `pass`, `upto`, `init`, `deinit`, `enum`, `throw`, `throws`, `rethrow`, and `catch`. `import` and `export` are contextual file-scope spellings, not reserved words; legacy spellings such as `fn`, `bool`, `int`, `defer`, `free`, `manual`, and `store` remain ordinary identifiers.

## Confirmed architecture

- C11 host compiler with `main.c` as its only compiled C translation unit.
- Header-based compiler subsystems in `compiler/include/swan/`.
- Swan-authored SIL builder and optimization stages in `compiler/sil/`, compiled into the final compiler by `build.sh`.
- UTF-8-aware, indentation-sensitive lexer and parser.
- AST semantic analysis, standard-library loading, C header declaration import, and function reachability.
- Typed, target-independent SSA IR with checked storage growth, overflow guards, SSA/dominance verification, operation-specific operand contracts, integer-only arithmetic typing, and leading-PHI validation.
- Typed-IR optimizer with constant folding, CFG simplification, algebraic simplification, PHI propagation, CSE, LICM, and worklist DCE.
- Shared SVM lowering/verification/optimization core feeding Swan-owned x86-64, AArch64, ARMv7, RISC-V64, WASM32, and WASM64 backends.
- Optional, explicit LLVM IR bridge for target triples supported by the installed toolchain.
- x86-64 AST compatibility backend for supported features not yet covered by typed IR.


The typed-IR verifier rejects malformed function/block storage, invalid operation/result combinations, pointer constants, non-integer arithmetic, misplaced PHIs, invalid call signatures, unused operands on fixed-arity operations, and malformed terminator fields. Dedicated negative tests cover these boundaries; builder APIs also reject null inputs and allocation-size overflow before mutating the IR.

## Source lowering

The general typed-IR generator currently covers the implemented scalar/control-flow subset: integer and boolean expressions, direct calls, locals, assignment, if/else, while, break/continue, and short-circuit AND/OR.

The Swan-authored SIL bridge is narrower. It accepts straight-line functions with one return expression, scalar parameters/literals, addition, subtraction, multiplication, equality, and signed-less-than. It uses a five-u32-word instruction record and currently limits the builder to 256 value IDs and 256 instructions; packed immediates are non-negative and limited to 16 bits. Functions outside this subset retain the general typed-IR path.

The x86-64 AST compatibility path retains features that are not yet lowered through typed IR, notably the supported u32 array forms, output/logging, and runtime-lazy imported modules.

## Memory and ownership

- Eligible local arrays use compiler-generated cleanup without runtime reference counting. Small fixed-size, non-escaping arrays use native stack-frame storage.
- The x86-64 AST compatibility backend now null-initializes eligible heap-array handles before conditional control flow, null-checks and clears handles during cleanup, and releases non-escaping arrays after their final top-level use when the AST liveness walk proves that no later statement references them.
- Branches and loops are treated conservatively as enclosing statements for last-use analysis. Early returns and the function epilogue can both emit cleanup safely because released handles are cleared.
- Escaping arrays remain excluded from last-use cleanup; ownership transfer and explicit borrow semantics are not yet implemented. The typed-IR backends do not yet share this array lifetime implementation.
- Manual allocation modifiers and the source-level `free(...)` intrinsic have been removed from the C11 parser and semantic checker. `free` remains an ordinary identifier; runtime-generated native cleanup may still call the platform allocator's `free` function. Internal legacy ownership fields remain technical debt pending the complete ownership pipeline.

## Backend status

| Backend | Verified implementation boundary |
|---|---|
| x86-64 System V Linux | Native executable/assembly; scalar typed-IR path and AST compatibility path for supported arrays, output, and lazy imports |
| AArch64 Linux | Swan-owned object/assembly for a limited scalar typed-IR subset |
| ARMv7 Linux, ARM state | Swan-owned object/assembly for a limited i32/i1 subset |
| RISC-V64 Linux | Swan-owned object/assembly for a limited scalar typed-IR subset |
| WASM32 | Swan-owned WebAssembly module for the implemented scalar/control-flow subset |
| WASM64 | Memory64 module emission for its implemented subset; runtime interoperability must be tested with a compatible engine |
| Explicit LLVM triple | LLVM IR, assembly, or relocatable object for the subset lowerable to Swan IR and supported by the installed toolchain |

SVM is the target-independent compiler engine, not a runtime target. The old IR interpreter and `.swvm` artifact have been removed. Cross-target object-format checks establish emission, not execution on the target CPU. The typed-IR backend subset is narrower than the full x86-64 compatibility path. C header imports and native external calls currently target x86-64 System V; the importer supports scalar integer/bool declarations, not general C types.

## Important limitations

- Struct/aggregate layout and ABI lowering are incomplete.
- Dotted paths parse, but full struct-instance field and method resolution is not complete.
- Floating-point source execution is not complete even though IR types and some portability emission paths represent float types.
- Arrays, slices, strings/output, and runtime-lazy imports are not yet generally represented in typed IR.
- Closures, full pointer/reference semantics, borrowing, ownership transfer, destructors, exception unwinding, and general heap objects are not implemented. Automatic last-use cleanup currently covers only eligible local arrays in the x86-64 AST compatibility backend; it is not yet a general ownership system.
- The frontend keyword registry contains syntax vocabulary beyond the implemented language subset. A recognized keyword does not imply complete parsing, semantic analysis, lowering, or runtime behavior.
- Full cross-target runtime linking and OS/platform runtime integration are incomplete.
- The IR machine-code backends are not yet equivalent to a globally register-allocated, scheduled optimizing backend.

## Compiler 2 (swanc) implementation status

Compiler 2 is not release-ready and is not yet a complete executable compiler. Compiler 1 remains the production/bootstrap compiler.

| Component | Current evidence | Release status |
|---|---|---|
| Lexer | UTF-8 scanner with caller-owned token storage; regression coverage through Compiler 1 | Partial |
| Parser / AST | Pratt parser for integer expressions and a small AST record format | Partial |
| Semantic analysis | Contract files plus a linear ownership-event verifier | Not integrated |
| Ownership | Checks declare/read/write/shared-borrow/exclusive-borrow/end/move/drop transitions and first-error diagnostics | Component slice only |
| SIL / SILGen | Architectural source files exist; complete verified AST lowering is not integrated | Not implemented |
| IR / IRGen | Initial linear instruction records and verifier | Partial |
| Optimization | Verified linear pass manager with conservative integer constant folding and DCE | Component slice only |
| SVM | Instruction-set and pass-manager contracts; complete pipeline integration is absent | Not implemented |
| Backend | Target/ABI/register-allocation/emission component contracts | Not implemented |
| Driver / linker | Orchestration and toolchain contracts; not wired into a complete compiler | Not implemented |
| Self-hosting | Compiler 1 can compile selected Compiler 2 source slices | Not achieved |

The ownership-event verifier is deliberately narrow. Its input is an already-linearized event stream; it does not derive events from the AST or control-flow graph, infer non-lexical borrow regions, perform path-sensitive branch joins, validate alias provenance, or insert deterministic drops. Its passing tests prove only the tested event-state transitions. They do not establish Rust-equivalent memory safety.

Do not publish Compiler 2 as production-ready until the complete source frontend, AST/CFG semantic analysis, ownership and lifetime model, SIL/SVM verifiers and passes, target backend, driver, linker, and repeatable stage-2/stage-3 bootstrap are implemented and tested. Keep Compiler 1 as the supported compiler until that gate is met.

## Test policy

The repository's top-level `test.sh` builds the compiler and runs header isolation, C11 smoke, C ABI, UTF-8, profiling, smoke, regression, Foundation, lazy-import, architecture, and keyword tests.

For each new language feature, add:
1. A positive parser and semantic test.
2. Negative tests for invalid forms and diagnostics.
3. IR/verifier tests if the feature enters typed IR.
4. Backend tests for each target that claims support.
5. Runtime execution tests where the target can be executed.
6. A benchmark only when making a performance claim.

Do not infer complete target support from a successful compile or object emission alone.
