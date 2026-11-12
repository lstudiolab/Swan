# Testing Swan

## Full test command

From the repository root:

```sh
sh test.sh
```

Requirements: a C11 compiler, POSIX shell, and host assembler/linker available through `cc`. The build script creates `build/swan` before running the tests.

## Top-level test sequence

The current `test.sh` runs:

1. `build.sh` — bootstrap and production compiler build.
2. `tests/headers.sh` — compile compiler headers independently.
3. `tests/c11-smoke.sh` — strict C11 host compilation smoke test.
4. `tests/ffi.sh` — native C header import and C ABI link/run test.
5. `tests/utf8.sh` — UTF-8 source validation and tokenization regressions.
6. `tests/profile.sh` — compiler profiling output and stage coverage.
7. `tests/smoke.sh` — basic source compile/run smoke tests.
8. `tests/regressions.sh` — language, semantic, memory, and backend regressions.
9. `tests/foundation.sh` — Foundation source module behavior.
10. `tests/lazy-imports.sh` — reachability-based runtime-lazy module behavior.
11. `tests/architecture.sh` — target/backend and architecture checks.
12. Compile and run `tests/keywords.c` — keyword/token registry smoke test.
13. `tests/swanc-optimizer.sh` — Compiler 2 verified optimizer slice.
14. `tests/swanc-ownership.sh` — Compiler 2 ownership-event verifier.

## Coverage areas

The source test suite covers compiler construction, parser/semantic diagnostics, UTF-8 handling, local bindings, function calls, control flow, arrays, bounds checks, output, Foundation imports, lazy module loading, Compiler 1 strict `manual var` ownership and legacy `store` compatibility. Compiler 2 ownership-event tests separately cover default ownership-state transitions and borrow conflicts; AST/CFG integration is not yet covered.

Compiler 2 tests currently cover its isolated lexer/parser/IR/optimizer slices and the linearized ownership-event verifier. These are component tests, not proof that Compiler 2's full frontend, semantic analysis, SIL, SVM, backend, or driver pipeline is executable.

The typed-IR tests cover SSA construction and verification, CFG and PHI invariants, scalar lowering, optimization passes, SVM execution, and module serialization. Architecture tests cover the implemented target emitters, including WebAssembly module validation and ELF object identification where configured by the test scripts.

The C ABI test compiles a C implementation to an object, imports its header declarations, links the object through Swan's native executable path, and checks the resulting process exit status.

## What a passing test means

- A parser test proves syntax acceptance or rejection, not runtime implementation.
- A semantic test proves the checked rule exercised by that case.
- An IR verifier test proves the relevant IR invariant.
- An object-header test proves object emission for that architecture, not execution or ABI correctness on that machine.
- A WebAssembly structural validation test proves module validity; runtime execution is a stronger check.
- A host SVM test proves interpreter behavior for the tested IR operations, not performance parity with native code.
- A successful compile does not establish that every language feature is implemented on every target.

## Cross-target policy

A backend is advertised only for the subset exercised by its tests. Cross-target execution and full system linking require suitable target runners and toolchains. Clang/LLVM may be used for the explicit `--target-triple` portability path; it is not a silent fallback for the Swan-owned backends.

## Adding a regression

For a new feature, add the narrowest useful positive and negative cases. Include source diagnostics, IR verification, generated artifact checks, and executable tests as appropriate. For optimizer changes, add before/after IR assertions and behavior tests. For ownership changes, test valid release, leak, double-free, use-after-free, aliasing, and branch/loop state consistency.
