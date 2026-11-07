# Testing Swan

## Full test command

Run from the repository root with `sh test.sh`.

The suite builds the compiler with C++20 warnings treated as errors, checks architecture boundaries, compiles headers independently, assembles generated native code, runs end-to-end programs, and executes regressions.

## Test layers

- Build: compile the native compiler with the selected C++ compiler.
- Header isolation: compile each public header as the only project include.
- Architecture: enforce subsystem dependency direction and the single translation-unit policy.
- Smoke: emit x86-64 assembly, assemble and link it, then compare program output. Includes nested control flow and multi-dedent cases.
- Regressions: reject invalid return and void-type cases; validate return branches, struct dependency diagnostics, string parameters, and native execution.

Every language change should include valid and invalid source examples, diagnostics checks, and generated-assembly execution tests when runtime behavior is involved.

Use `CXX=clang++ CC=clang sh test.sh` to select the compiler and assembler/linker driver.