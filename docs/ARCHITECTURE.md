# Compiler Architecture

Swan uses a layered, header-oriented C++20 compiler architecture. The organization takes two useful ideas from established compilers:

- Goose's fine-grained compiler headers and central driver.
- Swift's domain-oriented separation between basic infrastructure, syntax, semantic analysis, code generation, and frontend/driver responsibilities.

This is an architectural reference, not copied Goose or Swift source code.

## Repository layout

```text
main.cpp
compiler/include/swan/
├── Basic/       Tokens, source locations, diagnostics
├── Support/     Version and filesystem utilities
├── Lex/         Keyword classification and lexical scanning
├── AST/         Expression, declaration, statement, and program nodes
├── Parse/       Token cursor, declarations, statements, expressions
├── Sema/        Type rules, declarations, scopes, statements, expressions
├── CodeGen/     C type mapping, string escaping, expression/statement/program emission
└── Driver/      Command-line options, compiler orchestration, executable driver
```

Top-level headers such as `swan/Parser.hpp` and `swan/Compiler.hpp` remain as compatibility forwarding headers. New compiler code should include the subsystem header that owns the component.

## Compilation flow

```text
.swan source
    -> Lex
    -> Parse
    -> AST
    -> Sema
    -> CodeGen
    -> generated C
```

- **Lex** converts source characters into tokens.
- **Parse** builds the syntax tree from tokens.
- **AST** defines the compiler's syntax representation.
- **Sema** validates declarations, scopes, types, and expressions.
- **CodeGen** emits C from a semantically checked program.
- **Driver** handles command-line input/output and coordinates the compiler.

SIL generation, demand analysis, and optimization are intentionally absent until Swan's language semantics and frontend are stable.

## Translation-unit policy

The executable has one C++ translation unit: `main.cpp`. Implementations are currently provided by inline definitions in subsystem headers and included through the compiler's public component headers. This preserves the requested single-translation-unit build without CMake.

The build uses the system C++ compiler directly through `build.sh`; the project targets C++20 and treats compiler warnings as errors.

## Extension rules

1. Put a feature in the subsystem that owns its responsibility.
2. Split implementation by cohesive responsibility when a header becomes difficult to navigate.
3. Keep dependencies flowing toward lower-level interfaces; the driver coordinates but does not implement compiler semantics.
4. Do not add placeholder passes or claim a stage exists before it has executable behavior and tests.
5. Add a focused regression test for every language or compiler behavior change.
