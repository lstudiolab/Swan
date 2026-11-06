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

The tests build the native compiler, transpile sample Swan programs, compile generated C, execute the results, and check semantic errors.

## Project structure

The compiler uses a layered header architecture informed by Goose's focused compiler headers and Swift's subsystem-oriented organization. See [Compiler Architecture](docs/ARCHITECTURE.md) for the responsibilities and dependency boundaries.

```text
main.cpp
compiler/include/swan/
├── Basic/       Tokens, source locations, diagnostics
├── Support/     Version and filesystem utilities
├── Lex/         Keyword classification and lexical scanning
├── AST/         Expression, declaration, statement, and program nodes
├── Parse/       Parser core, declarations, statements, expressions
├── Sema/        Scope, declaration, statement, expression, and type analysis
├── CodeGen/     C type mapping and program, statement, expression emitters
└── Driver/      Command-line handling and compiler orchestration

examples/        Swan programs
tests/           Compiler smoke tests
build.sh         Direct C++20 build, no CMake
```

The compiler currently has one C++ translation unit, `main.cpp`. The subsystem headers contain inline implementations and are included through their owning component headers.
