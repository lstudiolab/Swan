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

The unified compiler driver is the main entry point:

~~~sh
build/swan check program.swan
build/swan emit-sil program.swan -o program.sil
build/swan emit-llvm program.swan -o program.ll
~~~

Focused stage tools are also available for debugging:

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
- Integer addition, subtraction, multiplication, remainder, and comparisons
- Unary negation and logical not
- Conditional and while-loop control flow, including break and continue; loop-carried assignments are not yet supported
- Integer and string logging
- Return statements

The compiler reports unsupported constructs rather than silently pretending to compile them. In particular, typed floating-point lowering, struct layout and member access, imports, array operations, for iteration, async/coroutine lowering, and finalized hold, store, and operate semantics still need implementation.

Function parameter annotations are retained in the AST and checked by semantic analysis. The initial LLVM ABI still represents supported parameters and values as 64-bit integers; floating-point signatures are rejected until their lowering is implemented.

## Tests

CTest covers parser, semantic analysis, SIL lowering, and LLVM emission. GitHub Actions additionally assembles the emitted LLVM IR and links/runs a native smoke-test executable.
