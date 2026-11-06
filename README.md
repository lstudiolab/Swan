# Swan

Swan is a programming language with a native C++20 compiler. The compiler parses and validates Swan programs, then emits portable C11 source.

The compiler implementation is C++. Python is not required to build, run, or test Swan. CMake is not used.

## Compiler pipeline

```text
Swan source (.swan)
        |
        v
Swan compiler (C++20)
  lexer -> parser -> semantic analysis -> C emitter
        |
        v
Generated C source (.c)
        |
        v
Optional external C compiler (Clang or GCC)
        |
        v
Native executable
```

Swan owns its language rules and compilation pipeline. C is the current output target; the Swan compiler itself does not invoke a C compiler.

## Build

Requirements:
- A C++20 compiler: Clang 15+, GCC 12+, or a compatible compiler.
- A C11 compiler is only needed to compile generated C programs or run the full smoke tests.
- A POSIX shell for the supplied shell scripts.

Build directly, without CMake:

```sh
./build.sh
```

The executable is written to `build/swan`. Select a compiler with `CXX=clang++ ./build.sh`.

## Use

```sh
./build/swan examples/hello.swan -o hello.c
cc -std=c11 hello.c -o hello
./hello
```

Without `-o`, generated C is written to standard output. Use `--version` or `--help` for command information.

## Install on Linux, macOS, or Termux

```sh
./install.sh
```

The installer builds and tests Swan, then installs the executable into `$PREFIX/bin` when `PREFIX` is set (as in Termux), or `~/.local/bin` otherwise. Ensure that directory is on your `PATH`.

On Windows, run `install.ps1` from a shell with Clang, GCC, or MSVC installed. It builds `build/swan.exe`.

## Current language support

The native compiler currently supports:
- Functions with typed parameters and return types.
- Primitive types: `int`, `float`, `bool`, `string`, and `void`.
- Struct declarations.
- `let`, `var`, and `const` local declarations.
- `if`, `else`, and `while` blocks.
- `return`, `break`, `continue`, and `pass`.
- Arithmetic, comparisons, boolean operators, assignments, direct function calls, and `log`.
- Source-positioned lexer, parser, and semantic diagnostics.

The compiler deliberately rejects language features that are not implemented. Imports, `for`, `loop`, asynchronous operations, and the final semantics of `hold`, `store`, `operate`, `clone`, and related ownership keywords remain future work. A keyword being recognized by the lexer does not mean its behavior is implemented.

## Tests

```sh
./test.sh
```

Tests build the C++ compiler, transpile representative Swan programs, compile the generated C with the host C compiler, run those programs, and verify that invalid programs are rejected.

## Repository layout

```text
lib/swan.cpp       Complete C++20 compiler implementation
examples/          Swan example programs
tests/             Native compiler smoke tests
build.sh           Direct C++ build; no CMake
install.sh         Linux/macOS/Termux installer
install.ps1        Windows build helper
```

The compiler implementation is kept in one source file as requested. Build and test scripts are separate from the compiler implementation.
