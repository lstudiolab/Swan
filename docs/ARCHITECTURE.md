# Compiler Architecture

Swan uses a layered, header-oriented C++20 compiler architecture.

## Repository layout

```text
main.cpp
compiler/include/swan/
├── Basic/       Source locations, tokens, and diagnostics
├── Support/     File and version utilities
├── Lex/         Token scanners and keyword classification
├── AST/         Expression, declaration, statement, and program nodes
├── Parse/       Parser and grammar routines
├── Sema/        Type, scope, declaration, and statement analysis
├── Frontend/    Lexing, parsing, and semantic-analysis orchestration
├── CodeGen/     Direct x86-64 assembly generation
├── Compiler/    Whole-compilation orchestration
└── Driver/      Command-line options and native linking
```

## Compilation pipeline

```text
.swan source
  -> Lexer
  -> Parser
  -> AST
  -> Semantic analysis
  -> x86-64 GNU assembly
  -> assembler + system linker
  -> native executable
```

The compiler does not emit C as an intermediate representation. The assembly backend emits GNU assembler syntax for the x86-64 System V ELF ABI. The driver invokes the configured `CC` tool (normally `cc`) in assembler mode to assemble and link the generated source.

## Backend responsibilities

- Semantic analysis resolves and annotates expression types before code generation.
- `AssemblyGenerator` lowers checked AST nodes into x86-64 instructions.
- Stack slots hold source-level locals and parameters; expression temporaries use the machine stack.
- Swan-to-Swan calls use a compiler-defined scalar calling convention. External libc calls follow System V ABI rules.
- String and floating-point constants are emitted into read-only data.
- The driver owns temporary files, tool invocation, executable output, and process execution.

## Target limitations

The initial backend targets x86-64 Linux ELF. It does not yet implement other architectures, aggregate-value ABI lowering, debug information, register allocation, or a target-independent intermediate representation. Unsupported constructs must fail with diagnostics rather than silently produce incorrect code.

## Translation-unit policy

The executable has one checked-in C++ translation unit: `main.cpp`. Implementations are provided by inline definitions in subsystem headers. The build uses C++20 directly through `build.sh`; CMake is not required.

## Engineering rules

1. Basic and Support remain independent of compiler phases.
2. Frontend owns language analysis and does not depend on code generation.
3. Code generation consumes a semantically checked AST.
4. Driver owns process-level behavior, not language semantics.
5. Every backend behavior requires a regression test.
6. Performance claims require reproducible benchmarks and verified output.