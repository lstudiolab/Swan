# Swan Compiler Roadmap

Swan is intended to be a self-contained compiled language with Swan-owned machine-code backends. It must not transpile to C or delegate code generation to LLVM/Clang.

## Current baseline

- Swan lexer, parser, and semantic analysis.
- Initial direct AST-to-x86-64 System V assembly backend.
- GNU assembler Intel syntax output.
- System assembler/linker used only to assemble Swan-emitted assembly and link executables.
- Unsupported architectures are rejected rather than silently delegated.

## Phase 1 — Complete the x86-64 backend

- Implement complete integer, boolean, string, float, aggregate, and pointer lowering.
- Complete calling conventions, stack alignment, callee/caller-saved register handling, and variadic calls.
- Implement robust stack-frame layout and local-variable lifetime handling.
- Add correct struct layout and member access.
- Add debug symbols, relocations, and platform ABI support.
- Add assembly-level and executable regression tests.

## Phase 2 — Swan intermediate representation

- Add a typed, target-independent low-level IR.
- Separate language semantics from machine-specific instruction selection.
- Add IR verification and stable serialization/dump formats.
- Represent control flow as basic blocks and explicit terminators.
- Add explicit calling convention, memory, and data-layout operations.

## Phase 3 — Swan optimizer

- Constant folding and compile-time evaluation.
- Constant propagation and copy propagation.
- Dead-code and unreachable-block elimination.
- Common-subexpression elimination.
- Loop-invariant code motion and strength reduction.
- Inlining with compile-time budgets.
- Liveness analysis and register allocation.
- Instruction scheduling and peephole optimization.

## Phase 4 — Additional Swan-owned backends

Implement and validate each backend independently:
- AArch64 / ARM64.
- ARMv7 and Thumb.
- RISC-V 64.
- WebAssembly 32/64.

Each backend must own instruction selection, register allocation, ABI lowering, and assembly/object conventions. No LLVM fallback is permitted.

## Phase 5 — Platform and device support

- Apple Darwin and iOS ABIs.
- Android ARM64 and ARMv7 ABIs.
- Linux GNU and musl ABIs.
- Windows x64 and ARM64 ABIs.
- WebAssembly WASI and browser environments.
- Target-specific runtime libraries and linker integration.

## Phase 6 — Runtime and standard library

- Strings, arrays, collections, iterators, and algorithms.
- File and console I/O, errors, process APIs, and math.
- Networking and concurrency.
- Memory ownership, allocation, and destruction.
- Platform abstraction and cross-platform tests.

## Phase 7 — Production hardening

- Fuzz lexer, parser, semantic analysis, IR, and every backend.
- Sanitizer and static-analysis CI.
- Reproducible builds and toolchain pinning.
- Differential tests and benchmark artifacts.
- Stable compiler interfaces, package manager, formatter, LSP, and debugger.

## Engineering rules

1. Never transpile Swan into C.
2. Never use LLVM or Clang as Swan's code-generation backend.
3. System assemblers and linkers may consume Swan-emitted assembly.
4. Never claim a target is supported until its own backend passes ABI and execution tests.
5. Every optimization requires correctness tests and reproducible performance measurements.
