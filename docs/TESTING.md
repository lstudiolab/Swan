# Testing Swan

## Full test command

Run `sh test.sh` from the repository root. Requirements: a C11 compiler and a host system assembler/linker available through `cc`.

## CI test layers

- Build: compile the C11 compiler from the sole `main.c` translation unit.
- Header isolation: compile every C header independently with strict C11 syntax checks.
- Existing source corpus: run the compiler, standard-library and regression suites on the host.
- Typed IR: verify SSA definitions, dominance, calls, memory operations, PHI predecessor coverage and terminators.
- Optimization: test constant folding, typed integer-width semantics, CFG simplification, unreachable-block pruning, algebraic simplification, CSE and DCE.
- IR generation: compile .swan source through semantic analysis, IR generation, optimization and SVM execution; cover calls, recursion, mutable locals, loops, branches and short-circuit logic.
- SVM: test interpreter behavior, typed integer overflow, .swvm serialization round-trips, truncation rejection and trailing-data rejection.
- x86-64 IR: execute generated native code and validate the scalar subset against expected results.
- WASM32: emit a .wasm module and instantiate/execute it with a WebAssembly runtime, including calls, recursion, loops, branches, mutable locals and PHI-based short-circuit behavior.
- WASM64: validate scalar memory64 module emission and section structure.
- AArch64, ARMv7 and RISC-V64: emit ELF objects and inspect the object headers to confirm target architecture.

## Cross-target policy

A target is not supported merely because the command-line parser accepts its name. Each target must have a Swan-owned generator and a test that validates its actual output format. Object-header tests establish code emission, not runtime ABI correctness on that CPU. Cross-target execution and full system linking must be tested on an appropriate native runner before being advertised as complete.

No external compiler backend fallback is allowed. Clang may be used as an assembler driver on Swan-generated target assembly; it must not generate code from C or another frontend.
