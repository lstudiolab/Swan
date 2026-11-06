# Swan compiler rewrite: Goose implementation

## Decision

The Swan compiler will be implemented in the **Goose programming language**. OCaml is not needed because Goose is a viable implementation language and already provides the file and command-line APIs needed by a source-to-source compiler.

This is a rewrite of the compiler implementation, not a change to Swan's syntax. Swan source remains `.swan`; the new compiler reads it and writes C source (`.c`) only.

## Required behavior

```text
swan input.swan -o output.c
```

- Input: Swan source.
- Output: generated C source.
- No LLVM IR.
- No SIL output exposed as a user command.
- No native executable generation.
- No invocation of MSVC, Clang, GCC, or another C compiler.
- A successful transpilation means the C file was written; it does not mean that C was compiled.

## Why Goose is viable

Goose is a general-purpose systems language with arrays, structs, functions, file I/O, command-line arguments, and C source generation. Its official compiler can run Goose programs on Windows using its bundled TinyCC backend, so users do not need to install a separate C compiler just to run the Swan transpiler.

The Goose project currently distributes prebuilt Windows x64 compiler releases. Its compiler implementation is itself currently C++, so this decision does **not** mean the Goose toolchain is self-hosting. Swan's compiler source will be Goose; the external Goose toolchain is used to run/build it.

Official references:
- https://aardappel.github.io/goose/
- https://aardappel.github.io/goose/docs/stdlib.html
- https://aardappel.github.io/goose/docs/command_line.html

## Migration strategy

The current C++ compiler remains in the repository as a reference until the Goose implementation has equivalent tested behavior. It must not be deleted or disconnected from the default build until the replacement passes the Swan regression suite.

Port in these stages:

1. **Driver and diagnostics** — read a `.swan` file, handle arguments, report source locations, and write output atomically.
2. **Lexer** — preserve indentation tokens, comments, ASCII and UTF-8 string delimiters, numbers, operators, and source locations.
3. **Parser and AST** — port declarations, expressions, functions, structures, and control flow without changing Swan grammar.
4. **Semantic analysis** — port scopes, symbol resolution, type checking, mutability checks, and diagnostics.
5. **C emitter** — generate portable C99 with deterministic names, explicit runtime helpers, and correct escaping.
6. **Regression tests** — compare lexer/parser/sema behavior against existing Swan fixtures and compile generated C in CI as a test only. The Swan command itself still only emits C.
7. **Cutover** — make the Goose implementation the only shipped compiler, update installers and VS Code tasks, then remove the legacy C++/SIL/LLVM implementation after parity is demonstrated.

## C emission contract

The first C backend should target portable C99. It must emit only constructs whose Swan semantics are defined and implemented. Unsupported Swan features must produce a source diagnostic; they must never silently emit incorrect C.

The output should include any small runtime helpers it needs, so a generated `.c` file is inspectable and self-contained where practical.

## Current status

This document records the selected architecture and migration plan. It is **not** a claim that the compiler has already been ported. The Goose frontend, semantic analyzer, and C emitter still need to be implemented and tested before the C++ compiler can be retired.
