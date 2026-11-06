# Swan

Swan is an indentation-based programming language with a C++17 compiler.

> **Development status:** Swan is an early compiler implementation. The documented frontend and backend subset is usable for experimentation, but the language and runtime are not yet feature-complete or ABI-stable. Do not use it for production workloads yet.

## Install Swan

The installer configures a Release build, compiles the compiler, runs the test suite, and installs the `swan` command. It does not require LLVM to build the compiler. A C++17 toolchain and CMake 3.20+ are required; Clang is needed separately only when compiling emitted LLVM IR into a native executable.

### macOS and Linux

Install CMake and a C++17 compiler using your system's package manager, then run:

```sh
git clone https://github.com/lstudiolab/Swan.git
cd Swan
sh install.sh
```

The default install location is `~/.local/bin/swan`. The installer adds `~/.local/bin` to your Bash or Zsh profile when possible. Restart your terminal after installation.

Examples of prerequisites:

- Ubuntu/Debian: `sudo apt install cmake build-essential`
- Fedora: `sudo dnf install cmake gcc-c++ make`
- macOS with Homebrew: `brew install cmake` (install Apple's Command Line Tools with `xcode-select --install` if needed)
- Termux: `pkg install cmake clang make`

### Windows (PowerShell)

Install CMake 3.20+ and a C++17 toolchain (Visual Studio Build Tools with the C++ workload is recommended). Then run:

```powershell
git clone https://github.com/lstudiolab/Swan.git
cd Swan
.\install.ps1
```

The default install location is `%USERPROFILE%\.local\bin`. The installer adds that directory to your user PATH. Open a new terminal after installation.

### Custom install location

Set `SWAN_INSTALL_PREFIX` before running the installer:

```sh
SWAN_INSTALL_PREFIX="$HOME/apps/swan" sh install.sh
```

PowerShell:

```powershell
$env:SWAN_INSTALL_PREFIX = "$HOME\apps\swan"
.\install.ps1
```

The executable is installed in the prefix's `bin` directory. To use a different build directory, set `SWAN_BUILD_DIR`. To skip tests (not recommended), set `SWAN_SKIP_TESTS=1`.

## Build manually

If you prefer to configure the project yourself:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix ~/.local
```

Ensure the install prefix's `bin` directory is on your `PATH`.

## Compiler commands

```sh
swan check program.swan
swan emit-sil program.swan -o program.sil
swan emit-llvm program.swan -o program.ll
clang program.ll -o program
```

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

First install the Swan compiler using the instructions above. The `swan` executable must be available on your shell `PATH`.

Then package and install the extension:

```sh
cd vscode
npm install
npm run package
code --install-extension swan-language-0.1.0.vsix
```

Open a `.swan` file. Use **Terminal → Run Task** and choose a task beginning with **Swan:**. The repository includes these tasks in `.vscode/tasks.json`; copy or adapt them in your own Swan project.

The initial extension is declarative editor support. It does **not** yet provide a language server, IntelliSense, go-to-definition, semantic highlighting, or inline compiler diagnostics. Those require a dedicated language-server implementation and a stable compiler diagnostic protocol.

## Compiler architecture

```text
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
```

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
