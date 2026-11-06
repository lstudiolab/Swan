# Goose compiler implementation

This directory contains the replacement Swan compiler, written in Goose.

The compiler contract is:

```text
swan program.swan -o program.c
```

It reads Swan and emits C source only. It does not invoke a C compiler or produce executables, LLVM IR, or SIL.

## Current implementation status

The first implementation slice is intentionally narrow and explicit:

- Reads a Swan source file and writes the C output atomically.
- Accepts `fn main():`.
- Supports `log "literal"` / `log 'literal'`, `pass`, and integer/expression text after `return`.
- Escapes C string literals.
- Rejects unsupported statements rather than silently generating incorrect code.

This is not yet full Swan language support. The next slices are the lexer/token model, full parser and AST, semantic analysis, and broader C emission. Each feature must have regression coverage before it is marked supported.

## Build and run

Use the official Goose compiler distribution for your platform:

https://github.com/aardappel/goose/releases

From this directory, run the source with Goose:

```text
goose run main.goose -- program.swan -o program.c
```

The exact invocation may vary by Goose release. The generated `.c` file is not compiled by Swan.
