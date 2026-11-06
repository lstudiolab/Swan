# Swan

Swan is a programming language with a Python-based source-to-source transpiler.

Status: the transpiler is an early implementation. It currently supports a deliberately small language subset and translates Swan source into C source.

## Syntax

Swan uses braces to delimit every code block. Indentation is for readability only.

```swan
fn main() -> int {
    log "Hello from Swan";
    return 0;
}
```

Statements may be separated by newlines or semicolons.

## Repository layout

```text
lib/
  swan.py          # Complete transpiler implementation in one file
tests/
  test_transpiler.py
examples/
  hello.swan
pyproject.toml
```

The transpiler's lexer, parser, semantic analysis, C source emitter, and command-line interface are all contained in `lib/swan.py`.

## Install

Python 3.10 or newer is required.

macOS, Linux, and Termux:

```sh
git clone https://github.com/lstudiolab/Swan.git
cd Swan
python3 -m pip install --user .
```

Windows PowerShell:

```powershell
git clone https://github.com/lstudiolab/Swan.git
cd Swan
py -m pip install --user .
```

If the `swan` command is not found after installation, add Python's user scripts directory to PATH and open a new terminal.

## Transpile Swan to C

```sh
swan examples/hello.swan -o hello.c
```

Without `-o`, generated C source is written to standard output.

Swan emits a `.c` source file only. It does not compile C, link programs, or generate executables. To run generated code, compile the C file separately:

```sh
cc hello.c -o hello
./hello
```

## Current language support

The current subset includes functions, typed parameters, primitive types, local variables, structs, if / else, while, return and loop-control statements, basic expressions, calls, and `log`.

Unsupported syntax is reported as an error instead of being silently accepted. Imports, for-loops, async operations, and the final behavior of `hold`, `store`, and `operate` are not implemented yet.

## Tests

```sh
PYTHONPATH=lib python3 -m unittest discover -s tests -v
```

CI also checks that emitted C can be compiled and run. That is a test of the generated output, not a stage performed by the Swan transpiler.

## Visual Studio Code

The `vscode` directory contains syntax highlighting, brace configuration, and snippets for Swan source files.
