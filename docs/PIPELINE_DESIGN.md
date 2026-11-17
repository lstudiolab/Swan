# Swan Compiler Pipeline Design

## Implemented pipeline

```text
.swan
  -> source reader and UTF-8 validation
  -> lexer
  -> parser and AST
  -> C-header importer and Swan module loader
  -> semantic analysis
  -> reachability analysis
  -> direct AST -> SVM typed SSA IR lowering
  -> SVM verifier
  -> target-independent optimization
  -> final SVM verification
  -> Swan-owned target code generation or explicit LLVM portability bridge
  -> assembly / object / WebAssembly emission
  -> assembler and linker where applicable
```

The production compiler is Compiler 1, implemented in C11. `build.sh` compiles it directly. No bootstrap or second compiler build stage exists.

The x86-64 AST compatibility route remains for implemented features not yet represented in typed IR. It is selected by a source-feature check before IR lowering; an IR or verifier failure is always a hard error.

## Stage contracts

| Stage | Input | Output | Contract |
|---|---|---|---|
| Reader | Source path | UTF-8 bytes/text | Reject unreadable input and preserve source contents |
| Lexer | UTF-8 source | Token stream | Validate UTF-8 and preserve token/source locations |
| Parser | Tokens | AST | Reject malformed syntax; allocate nodes within the program lifetime |
| C-header importer | Header path and AST | External declarations | Import only the documented C ABI subset |
| Module loader | Import declarations | Loaded module functions | Resolve canonical module identities and avoid duplicate loads |
| Semantic analysis | Project AST | Checked program | Validate supported names, scopes, declarations, calls, and control flow |
| Reachability | Checked program | Reachability map | Identify functions/modules reachable from the entry point |
| SVM lowering | Checked AST | Typed SSA IR | Represent each supported operation or return a diagnostic |
| IR verifier | Typed SSA IR | Verified IR or diagnostic | Enforce SSA, CFG, PHI, call, type, and terminator invariants |
| IR optimizer | Verified IR | Optimized IR | Run bounded, semantics-preserving passes and preserve verifier invariants |
| Target codegen | Optimized IR and target | Assembly/module/object | Lower only operations supported by the selected backend |
| AST compatibility codegen | Checked AST and x86-64 target | x86-64 assembly | Handle the supported feature subset not yet in typed IR |
| Emitter | Assembly buffer | Output stream | Serialize output without semantic changes |
| Linker | Assembly/object and toolchain | Object/executable | Invoke platform tools and report failures without masking diagnostics |

## Typed IR and verifier

SVM IR uses function-local SSA value IDs, typed instructions, basic blocks, explicit terminators, and PHI incoming edges. The verifier checks definition uniqueness, use-before-definition, dominance, branch targets, terminators, PHI predecessor coverage and types, operand types, and call signatures.

A supported IR opcode does not imply that every source construct or target backend can generate it. Unsupported source/target combinations must produce precise diagnostics.

## Optimization ordering

The current typed-IR optimizer includes width-aware constant folding, constant-select and constant-branch folding, CFG simplification and unreachable-block removal, algebraic simplification, trivial-PHI copy propagation, dominance-aware CSE, conservative loop-invariant code motion, post-hoisting CSE, and worklist dead-code elimination.

The pass manager repeats its ordered pass list until a full round makes no changes, with a hard maximum of 350 pass executions. Potentially trapping operations and observable memory effects must not be discarded. Release builds avoid redundant whole-module verification at every pass boundary; debug builds verify each mutating pass result.

## Profiling

`--profile` reports wall-time cost for frontend and backend phases, IR verification, total optimization, and each optimization pass. The GitHub Actions pipeline benchmark adds repeated heavy workloads and peak RSS measurements. Pass timings are nested inside the optimization total.

## Target boundaries

- x86-64 System V: native executable and assembly; typed IR plus AST compatibility path.
- AArch64, ARMv7, RISC-V64: Swan-owned limited scalar IR object/assembly.
- WASM32 and WASM64: Swan-owned limited scalar WebAssembly emitters.
- LLVM target triple: explicit portability path for the supported typed-IR subset and installed Clang/LLVM targets.

Object emission is not equivalent to native execution or complete platform ABI support. Cross-target runtime integration remains separate work.

## Required invariants

1. Do not transpile Swan to C.
2. Keep the native backend path Swan-owned.
3. Keep the LLVM bridge explicit through `--target-triple`; it is not a silent fallback.
4. Do not silently lower unknown AST/IR kinds to empty or zero code.
5. Verify IR before backend use and after optimizer transformations.
6. Keep language, IR, and backend support claims distinct.
7. Measure compile time and memory with repeatable workloads before claiming improvements.
