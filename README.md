# Swan

Swan is an indentation-based programming language with a C++17 compiler.

> **Development status:** Swan is an early compiler implementation. The documented frontend and backend subset is usable for experimentation, but the language and runtime are not yet feature-complete or ABI-stable. Do not use it for production workloads yet.

## Requirements

- CMake 3.20 or newer
- A C++17 compiler (GCC 10+, Clang 12+, or a supported MSVC toolchain)
- LLVM 15+ and Clang for validating and compiling emitted LLVM IR
- Node.js 20+ and npm only when packaging the VS Code extension

## Build from source

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
~~~

To install the compiler to the configured prefix:

~~~sh
cmake --install build --prefix ~/.local
~~~

Ensure `~/.local/bin` is on your `PATH`.

## Compiler commands

~~~sh
swan check program.swan
swan emit-sil program.swan -o program.sil
swan emit-llvm program.swan -o program.ll
clang program.ll -o program
~~~

The compiler exits non-zero when a stage reports an error. `check` performs lexer, parser, and semantic analysis. LLVM emission additionally lowers to SIL, optimizes, verifies, and emits LLVM IR.

Focused tools are also built for compiler development: `swan-lex`, `swan-parse`, `swan-check`, `swan-sil`, and `swan-llvm`.

## Visual Studio Code

Swan includes a VS Code extension under `vscode/` with:

- `.swan` file association
- Syntax highlighting for Swan keywords, strings, numbers, operators, and built-ins
- Indentation and bracket configuration
- Code snippets for functions, structures, variables, conditions, loops, and logging
- Workspace tasks to check a file and emit SIL or LLVM IR

### Install the extension from source

First build and install the Swan compiler using the instructions above. The `swan` executable must be available on your shell `PATH`.

Then package and install the extension:

~~~sh
cd vscode
npm install
npm run package
code --install-extension swan-language-0.1.0.vsix
~~~

Open a `.swan` file. Use **Terminal → Run Task** and choose a task beginning with **Swan:**. The repository includes these tasks in `.vscode/tasks.json`; copy or adapt them in your own Swan project.

The initial extension is declarative editor support. It does **not** yet provide a language server, IntelliSense, go-to-definition, semantic highlighting, or inline compiler diagnostics. Those require a dedicated language-server implementation and a stable compiler diagnostic protocol.

## Compiler architecture

~~~text
Source
  |
  v
Lexer -> Parser -> AST -> Semantic Analysis
  |
  v
Swan Intermediate Language (SIL)
  |  - lowering
  |  - verification
  |  - optimization
  v
LLVM IR Backend
  |
  v
LLVM toolchain -> Native executable
~~~

## Current backend coverage

The current SIL and LLVM path is incremental, not full language coverage.

Supported in the current lowering path:

- Function declarations and direct function calls
- Integer and boolean literals
- Local variables represented through SSA values
- Integer addition, subtraction, multiplication, remainder, and comparisons
- Unary negation and logical not
- Conditional and while-loop control flow, including break and continue; loop-carried assignments are not yet supported
- Integer and string logging
- Return statements

Unsupported or incomplete areas include typed floating-point lowering, struct layout and member access, imports, array operations, for iteration, async/coroutine lowering, string concatenation/comparison, top-level executable statements, and finalized `hold`, `store`, and `operate` semantics. Division is deferred because its Swan result type is floating point.

Function parameter annotations are retained in the AST and checked by semantic analysis. The initial LLVM ABI represents supported parameters and values as 64-bit integers; floating-point signatures are rejected until their lowering is implemented. Unsupported constructs should produce diagnostics rather than silently compiling as if implemented.

## Tests and continuous integration

CTest covers parser, semantic analysis, SIL lowering, verification, and LLVM emission. GitHub Actions builds the compiler, runs CTest, validates emitted LLVM IR with `llvm-as`, optimizes it with `opt`, and links/runs a native smoke test with Clang.

A configured CI workflow is not evidence that a particular revision passed. Check the Actions tab for the status of the commit you intend to use.
