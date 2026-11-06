# Testing Swan

## Full test command

Run from the repository root:

```sh
sh test.sh
```

The suite builds the compiler with C++20 warnings treated as errors, checks architecture boundaries, compiles headers independently, runs end-to-end transpilation tests, and executes focused regressions.

## Test layers

- **Build:** compile the native compiler with the selected C++ compiler.
- **Header isolation:** compile each public header as the only project include.
- **Architecture:** enforce subsystem dependency direction and the single translation-unit policy.
- **Smoke:** compile Swan examples to C11, compile the C with a system C compiler, and compare program output.
- **Regressions:** reject invalid return and void-type cases; validate return branches, struct dependency ordering, cycle diagnostics, and C identifier isolation.

## Adding a test

Every language change should include:

1. A valid source program demonstrating the behavior.
2. An invalid source program for relevant failure conditions.
3. A check that diagnostics reject invalid input.
4. A generated-C compile and execution test when runtime behavior is involved.
5. A regression fixture for every bug fixed.

Use `CC` and `CXX` to select the C and C++ compilers:

```sh
CXX=clang++ CC=clang sh test.sh
```

## Future hardening

The suite should grow to include lexer/parser fuzzing, sanitizers, property-based tests, cross-platform CI, and compile-time and memory benchmarks. These are not claimed to be complete by the current smoke and regression scripts.
