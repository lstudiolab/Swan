# Swan

Swan is an indentation-based programming language and compiler written in C++17.

## Build

Requirements:
- CMake 3.20+
- A C++17 compiler
- LLVM command-line tools and Clang for validating and compiling emitted LLVM IR

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
~~~

## Compiler tools

The repository currently exposes focused tools for each compiler stage:

~~~sh
# Lex source into tokens
build/swan-lex program.swan

# Parse source into an AST
build/swan-parse program.swan

# Run semantic analysis
build/swan-check program.swan

# Lower supported functions into Swan SIL
build/swan-sil program.swan

# Emit LLVM IR
build/swan-llvm program.swan -o program.ll

# Assemble, optimize, and compile the emitted IR with LLVM tools
llvm-as program.ll -o program.bc
opt -O2 program.bc -o program.opt.bc
clang program.opt.bc -o program
~~~

## Compiler architecture

~~~text
Source
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
Semantic Analysis
  |
  v
Swan Intermediate Language (SIL)
  |  - lowering
  |  - verification
  |  - constant folding
  |  - dead-code elimination
  v
LLVM IR Backend
  |
  v
LLVM assembler / optimizer / native toolchain
  |
  v
Executable
~~~

## Current backend coverage

The current SIL and LLVM path is an incremental implementation, not yet full language coverage.

Supported in the current lowering path:
- Function declarations and direct function calls
- Integer and boolean literals
- Local variables represented through SSA values
- Integer arithmetic and comparisons
- Unary negation and logical not
- Conditional and while-loop control flow without loop-carried assignments
- Integer and string logging
- Return statements

The compiler reports unsupported constructs rather than silently pretending to compile them. In particular, typed floating-point lowering, struct layout and member access, imports, array operations, for iteration, break/continue, and finalized hold, store, and operate semantics still need implementation.

Function parameter type annotations are parsed by the frontend but are not yet retained in the AST; consequently, the initial backend currently uses 64-bit integer calling conventions for lowered values. This is a known limitation, not a language-level type rule.

## Tests

CTest covers parser, semantic analysis, SIL lowering, and LLVM emission. GitHub Actions additionally assembles the emitted LLVM IR and links/runs a native smoke-test executable.
