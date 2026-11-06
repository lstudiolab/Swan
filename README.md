![Swan programming language logo](assets/swan-logo.svg)

# Swan

Swan is a programming language with a native C++20 compiler. It currently parses and validates Swan source, then emits portable C11 source.

The compiler implementation is C++. Python and CMake are not required.

## Compiler pipeline

```text
.swan source
    |
    v
Lexer
    |
    v
Parser
    |
    v
AST
    |
    v
Sema (semantic analysis)
    |
    v
C source generation
    |
    v
.c output

Planned after the language and frontend stabilize:
    SIL generation -> SIL demand -> SIL optimization
```

SIL is deliberately not implemented yet. The immediate priority is a correct and stable language frontend and C output backend. The future SIL stages will be introduced only after the Swan language design is stable.

## Build without CMake

Requirements: a C++20 compiler (Clang 15+, GCC 12+, or compatible).

```sh
sh build.sh
```

This compiles the single `main.cpp` translation unit, using the stage headers, and writes `build/swan`.

## Use

```sh
./build/swan examples/hello.swan -o hello.c
cc -std=c11 hello.c -o hello
./hello
```

Swan itself emits C source. It does not invoke the C compiler.

## Install

Linux, macOS, and Termux:

```sh
sh install.sh
```

Windows: run `install.ps1` with a C++20 compiler installed.

## Implemented frontend

- Lexer with source locations, comments, strings, numbers, operators, and keyword recognition.
- Recursive-descent parser and precedence-based expression parser.
- Explicit AST nodes for declarations, statements, and expressions.
- Semantic analysis for primitive types, function declarations, parameter and return types, variable scopes, mutability, calls, and control-flow conditions.
- C11 source generation for the supported language subset.

Current syntax includes functions, typed parameters, structs, local `let`/`var`/`const`, `if`/`else`, `while`, return, loop control, arithmetic, comparisons, boolean operations, calls, and `log`.

Recognizing a keyword does not mean its semantics are complete. Imports, `for`, `loop`, async operations, and the ownership/recovery semantics of `hold`, `store`, `operate`, and `clone` are not implemented yet. Unsupported constructs must not be treated as stable language features.

## Tests

```sh
sh test.sh
```

The test suite builds the native compiler, compiles every header independently to check include boundaries, transpiles sample Swan programs, compiles generated C, executes the results, and checks semantic errors.

## Project structure

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for subsystem responsibilities and dependency rules.

```text
main.cpp
compiler/include/swan/
  Basic/       Tokens, source locations, diagnostics
  Support/     Filesystem and version utilities
  Lex/         Keyword, character, identifier, number, string, punctuation scanners
  AST/         AST node definitions
  Parse/       Parser core, declarations, statements, expressions
  Sema/        Scope, declaration, statement, expression, type analysis
  Frontend/    Lexing, parsing, and semantic-analysis orchestration
  Compiler/    Whole-compilation pipeline
  CodeGen/     C type mapping and focused C emitters
  Driver/      Command-line parsing and executable driver

examples/
tests/
build.sh
```
