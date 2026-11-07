# Testing Swan

## Full test command

Run `sh test.sh` from the repository root. Requirements: C11 compiler and a host system assembler/linker available through `cc`.

## Test layers

- Build: compile the C11 compiler from the sole `main.c` translation unit.
- Header isolation: compile every C header independently with strict C11 syntax checks.
- Architecture: verify x86-64 assembly emission and executable output for the currently ported C subset.
- Smoke: exercise functions, local variables, strings, integer arithmetic, and native execution in the currently ported C subset.
- Regressions: verify invalid source is rejected and semantic regressions remain covered.

Every language change requires valid and invalid examples, diagnostic checks, assembly inspection where relevant, and executable tests.

## Cross-target policy

A target is not supported merely because the command-line parser accepts its name. The corresponding Swan-owned backend must generate valid assembly and pass ABI, object, and execution tests on an appropriate runner. No external compiler backend fallback is allowed; the system assembler/linker may consume Swan-generated assembly.
