# Compiler Architecture

Swan uses a layered, header-oriented C++20 compiler architecture. Its organization takes two useful ideas from established compilers:

- **Goose:** focused headers for concrete compiler responsibilities and a small central driver.
- **Swift:** domain-oriented subsystem boundaries, with compiler infrastructure, syntax, semantic analysis, frontend orchestration, and backend work kept distinct.

This is an architectural adaptation, not copied Goose or Swift source code.

## Repository layout

```text
main.cpp
compiler/include/swan/
├── Basic/       Source locations, tokens, and diagnostics
├── Support/     Version and filesystem utilities
├── Lex/         Keyword classification and focused token scanners
├── AST/         Expression, declaration, statement, and program nodes
├── Parse/       Parser core, declarations, statements, expressions
├── Sema/        Type rules, scopes, declarations, statements, expressions
├── Frontend/    Lexing, parsing, and semantic-analysis orchestration
├── CodeGen/     C types, literal escaping, expression/statement/program emission
├── Compiler/    Whole-compilation orchestration and backend selection
└── Driver/      Command-line options and executable driver
```

Top-level headers such as `swan/Parser.hpp` and `swan/Compiler.hpp` remain thin compatibility facades. The old `swan/Driver/Compiler.hpp` path is also retained as a forwarding header. New compiler code should include the subsystem header that owns the component.

## Subsystem responsibilities

- **Basic** owns source locations, tokens, and diagnostics. It must not depend on compiler phases.
- **Support** owns small utilities such as file reading, file writing, and version information.
- **Lex** converts source characters into tokens. Identifier, number, string, and punctuation scanning are separate responsibilities.
- **AST** defines the syntax representation. It does not depend on the parser or semantic analyzer.
- **Parse** consumes tokens and constructs the AST. Expression precedence and grammar routines are separated from parser state management.
- **Sema** validates declarations, scopes, types, statements, and expressions. It does not emit target code.
- **Frontend** coordinates the language-analysis stages and returns a checked program.
- **CodeGen** consumes a semantically checked program and emits C11. Type mapping, literal escaping, and expression/statement/program emission are separated.
- **Compiler** coordinates the frontend and the currently supported C backend.
- **Driver** handles command-line input/output and process-level behavior. It does not own language semantics or backend implementation.

## Dependency direction

The dependency direction is intentionally one-way:

1. Basic and Support provide low-level facilities.
2. Lex, AST, Parse, and Sema implement language representation and analysis.
3. Frontend composes those language stages.
4. CodeGen consumes the checked AST.
5. Compiler composes Frontend and CodeGen.
6. Driver handles process arguments and file I/O through Compiler.

A lower-level subsystem must not depend on Driver. Frontend must not depend on CodeGen. This keeps semantic analysis reusable by future tools such as a formatter, language server, or alternate backend.

## Translation-unit policy

The executable has one checked-in C++ translation unit: `main.cpp`. Implementations are provided by inline definitions in subsystem headers and included through their owning component headers. This preserves the requested single-translation-unit build without CMake.

The build uses the system C++ compiler directly through `build.sh`; the project targets C++20 and treats compiler warnings as errors.

## Header rules

1. Each header has one primary responsibility and lives in its owning subsystem.
2. Public component headers declare the component; focused headers define related implementation routines.
3. Headers include their direct dependencies and use `#pragma once`.
4. The header smoke test compiles every header as the only include in a temporary translation unit.
5. Do not add placeholder headers for future features.
6. Keep compatibility facades thin while existing examples and tools still use them.
7. Add a focused regression test for every language or compiler behavior change.

## Current compilation pipeline

```text
Source text
  -> Lexer
  -> Parser
  -> AST
  -> Semantic analysis
  -> C source generation
```

SIL generation, demand analysis, and SIL optimization are future phases. They must not be represented by empty or pretend implementations before Swan's language semantics are stable.
