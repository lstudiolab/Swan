# Testing Swan

## Full test command

Run from the repository root with sh test.sh.

The suite builds the compiler with C++20 warnings treated as errors, checks architecture boundaries, compiles headers independently, runs end-to-end transpilation tests, and executes regressions.

## Test layers

- Build: compile the native compiler with the selected C++ compiler.
- Header isolation: compile each public header as the only project include.
- Architecture: enforce subsystem dependency direction and the single translation-unit policy.
- Smoke: compile indentation-based Swan programs to C11, compile the C, and compare program output.
- Regressions: reject invalid return and void-type cases; validate return branches, struct dependency ordering, cycle diagnostics, and identifier isolation.

Every language change should include valid and invalid source examples, diagnostics checks, and generated-C execution tests when runtime behavior is involved.

Use CXX=clang++ CC=clang sh test.sh to select compilers.
