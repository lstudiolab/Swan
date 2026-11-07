# Testing Swan

## Full test command

Run from the repository root with `sh test.sh`.

Requirements: a C++20 compiler and Clang/LLVM with the target backends being tested.

## Test layers

- Build: compile the unified Swan compiler.
- Header isolation: compile each public header independently.
- Architecture: enforce subsystem dependency direction.
- Smoke: compile and run native programs through LLVM IR and the host backend. Includes nested control flow, strings, functions, arrays, and arithmetic.
- Regressions: reject invalid return and void-type cases; validate return branches, struct dependencies, and string parameters.
- Targets: emit assembly for x86-64, AArch64, ARMv7, RISC-V 64, WebAssembly, Apple, Android, and Windows target triples.

Every language change should include valid and invalid source examples, diagnostic checks, and generated-code execution tests where runtime behavior is involved.

Use `SWAN_CLANG=/path/to/clang sh test.sh` to select the LLVM driver.
