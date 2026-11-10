# Swan Compiler Pipeline Design

This document defines the current stage boundaries and invariants.

## Implemented pipeline

```text
.swan
  -> source reader / UTF-8 validation
  -> lexer
  -> parser / AST
  -> C header import + Swan module loader
  -> semantic analysis
  -> reachability analysis
  -> lowering:
       Swan SIL bridge (eligible scalar functions)
       general AST -> typed SSA IR
       AST -> x86-64 compatibility backend
  -> typed-IR verification and optimization
  -> Swan-owned target codegen or explicit LLVM bridge
  -> emitter / object or module writer
  -> assembler / linker where applicable
```

The SIL route is an optional narrow lowering path, not a mandatory pipeline stage. The x86-64 AST compatibility route remains for implemented features not yet lowered into typed IR. Other native backends consume typed IR.

## Stage contracts

| Stage | Input | Output | Contract |
|---|---|---|---|
| Reader | Source path | UTF-8 bytes/text | Reject unreadable source; preserve source contents |
| Lexer | UTF-8 source | Token stream | Validate UTF-8; emit indentation and source-location tokens |
| Parser | Tokens | AST | Parse syntax and construct arena-owned nodes |
| C header importer | Header path + AST | External function declarations | Import only supported scalar prototypes through Clang |
| Module loader | Import declarations | Loaded module AST/functions | Resolve source modules and track module ownership |
| Semantic analysis | Program AST | Checked program | Validate supported names, scopes, declarations, calls, and strict manual ownership |
| Reachability | Checked program | Reachability map | Identify functions/modules reachable from the entry point |
| SIL bridge | Eligible scalar AST functions | Typed IR function bodies | Use Swan-authored bounded scalar SIL; otherwise leave general IR lowering intact |
| General IR generation | Checked AST | Typed SSA IR | Lower the implemented scalar/control-flow subset without target-specific instructions |
| IR verifier | Typed IR | Verified IR or diagnostic | Enforce SSA, CFG, dominance, PHI, type, and call invariants |
| IR optimizer | Verified IR | Optimized verified IR | Apply semantics-preserving scalar/CFG passes; verify transformations |
| Target codegen | Optimized IR + target | Assembly/module/object | Lower only operations supported by that backend |
| AST compatibility codegen | Checked AST + x86-64 target | x86-64 assembly | Handle the supported legacy feature subset not yet in IR |
| Emitter | Assembly buffer | Output stream | Serialize assembly without semantic changes |
| Linker | Assembly/object + toolchain | Object/executable | Invoke system toolchain; no C transpilation |
| SVM | Verified IR or `.swvm` | Execution result | Interpret bounded, verified IR with fuel/call-depth controls |

## Typed IR and verifier

IR uses function-local SSA value IDs, typed instructions, basic blocks, explicit terminators, and PHI incoming edges. The IR type set includes void, i1/i8/i16/i32/i64, f32/f64, and pointers. The operation set includes constants, arithmetic/division/remainder, bitwise operations, shifts, comparisons, select, PHI, alloca/load/store, and calls.

The verifier checks definition uniqueness and bounds, use-before-definition, dominance, branch targets, terminators, PHI predecessor coverage and types, operand types, and call signatures. A supported IR opcode does not imply source lowering or code generation support on every target.

## Optimization ordering

The current typed-IR optimizer includes width-aware constant folding, constant-select folding, constant-branch/CFG simplification, unreachable-block pruning, algebraic simplification, trivial-PHI propagation, dominance-aware CSE, conservative LICM, post-hoisting CSE, and worklist DCE.

Potentially trapping operations and observable memory effects must not be discarded as dead pure work. Verification must run around transformations so invalid SSA/CFG state is not passed to a backend.

## SIL contract

SIL source stages live in `compiler/sil/` and are compiled into the production compiler during bootstrap. Each record is five u32 words: opcode/type, destination, left operand, right operand, and immediate/parameter index.

The implemented builder is bounded to 256 value IDs and 256 instructions. Its supported records are parameters, constants, add/subtract/multiply, equality, signed-less-than, copy, and return; packed constants currently accept non-negative immediates through 65535. The AST bridge accepts only straight-line, single-return scalar functions. CFG, branches, variables, memory, general calls, loops, and imports remain outside SIL. The general typed-IR generator is the fallback for other IR-supported functions.

## Target boundaries

- x86-64 System V: native executable/assembly; typed IR plus AST compatibility route.
- AArch64, ARMv7, RISC-V64: Swan-owned limited scalar IR object/assembly.
- WASM32 and WASM64: Swan-owned limited scalar WebAssembly emitters.
- SVM: verified IR interpreter and versioned `.swvm` artifact.
- LLVM target triple: explicit portability route for the supported typed-IR subset and installed Clang/LLVM targets.

Object emission is not equivalent to native execution or complete platform ABI support. Cross-target runtime integration remains a separate task.

## Required invariants

1. Do not transpile Swan to C.
2. Keep native code generation Swan-owned.
3. Use LLVM only through the explicit target-triple path.
4. Do not silently lower unknown AST/IR kinds to empty or zero code.
5. Verify IR before backend use and after optimizer transformations.
6. Keep source-language, IR, and backend support claims distinct.
7. Add positive, negative, verifier, and runtime tests for new constructs.
