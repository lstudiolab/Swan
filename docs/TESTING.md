# Testing Swan

## Full test command

Run `sh test.sh` from the repository root. Requirements: C++20 compiler and a host system assembler/linker available through `cc`.

## Test layers

- Build: compile the Swan compiler without LLVM or Clang.
- Header isolation: compile public headers independently.
- Architecture: verify Swan-owned x86-64 assembly output, object generation, linking, execution, and explicit rejection of unimplemented targets.
- Smoke: exercise functions, local variables, nested control flow, strings, arrays, arithmetic, and source diagnostics.
- Regressions: verify invalid source is rejected and semantic regressions remain covered.

Every language change requires valid and invalid examples, diagnostic checks, assembly inspection where relevant, and executable tests.

## Cross-target policy

A target is not supported merely because the command-line parser accepts its name. The corresponding Swan-owned backend must generate valid assembly and pass ABI, object, and execution tests on an appropriate runner. No external compiler backend fallback is allowed.
