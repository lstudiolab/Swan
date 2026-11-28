# swanc

swanc is the C++ compiler for Swan. It compiles **one .swan source file per invocation**. The frontend validates Swan and lowers the accepted syntax to C in memory; Clang reads that generated source from standard input and directly produces an object file or executable. No generated .c file is written to disk.

## Build

```sh
cmake -S swanc -B build
cmake --build build
```

Clang must be installed and available on PATH (or supplied with --clang).

## Use

```sh
# Check syntax and declarations
./build/swanc --check examples/hello.swan

# Compile and link an executable
./build/swanc examples/hello.swan -o hello

# Compile one source file to an object file
./build/swanc -c examples/hello.swan -o hello.o

# Compile with optimization and debug information
./build/swanc examples/hello.swan -O2 -g -o hello
```

For a multi-file program, compile each source file independently to an object file, then link the objects with the platform linker/Clang. Cross-file declarations currently need to be declared using Swan's extern syntax once its declaration grammar is implemented; automatic project/module imports are not implemented yet.

## Pipeline

```text
Source.swan
  -> Lexer
  -> Parser
  -> Sema
  -> Type checking (in progress)
  -> Swan IR (planned)
  -> Code generation
  -> Clang (C from stdin)
  -> .o or executable
```

## Current language support

The bootstrap frontend currently supports:
- Function declarations with primitive parameter and return types.
- let, var, and const local declarations.
- return, if / else, and while.
- Integer/floating literals, booleans, names, unary operators, arithmetic, comparisons, and grouped expressions.
- Duplicate function and parameter checks and primitive signature validation.

The current parser does **not** yet implement the full Swan language. Function calls, imports, structs, enums, assignment, complete type checking, control-flow validation, and ownership/lifetime analysis remain outstanding. The listed reserved words are tokenized but are not all implemented as language constructs.

## Backend and performance

Clang is used as a backend driver, not as the Swan frontend. The compiler generates C text in memory and streams it directly to Clang using stdin. This avoids creating a temporary C source file, but it does not remove Clang's C parsing/code-generation cost. A future direct machine-code backend can replace this boundary without changing the lexer/parser/semantic frontend.

Swan has no language-level garbage collector or implicit general-purpose heap allocation. The exact rules for references, aggregate storage, lifetimes, and resource cleanup still need to be specified and enforced before the language can be considered complete.
