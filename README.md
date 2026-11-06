# Swan

Swan is a programming language with a Python-based source-to-source transpiler.

Status: the Python transpiler is an early implementation. It currently supports a deliberately small language subset and transpiles Swan source into C source.

## Syntax

Swan uses braces to delimit every code block. Indentation is for readability only.

    fn main() -> int {
        log "Hello from Swan";
        return 0;
    }

A block begins with { and ends with }. Statements may be separated by newlines or semicolons.

## Install

Python 3.10 or newer is required. No C++ compiler, CMake, Goose, or LLVM installation is required to run the Swan compiler.

macOS, Linux, and Termux:

    git clone https://github.com/lstudiolab/Swan.git
    cd Swan
    python3 -m pip install --user ./Compiler/Python

Windows PowerShell:

    git clone https://github.com/lstudiolab/Swan.git
    cd Swan
    py -m pip install --user .\Compiler\Python

If the swan command is not found after installation, add Python's user scripts directory to PATH and open a new terminal.

## Transpile Swan to C

    swan program.swan -o program.c

Without -o, generated C source is written to standard output.

Swan only emits a .c source file. It does not compile C, link programs, or generate executables. To run generated code, compile the C file separately:

    cc program.c -o program
    ./program

## Current transpiler support

The Python transpiler contains a lexer, recursive-descent parser, and C source emitter. The current subset includes functions, typed parameters, primitive types, local variables, structs, if / else, while, return and loop-control statements, basic expressions, calls, and log.

Unsupported syntax is reported as an error instead of being silently accepted. Imports, for-loops, async operations, and the final behavior of hold, store, and operate are not implemented yet.

## Transpiler layout

    Compiler/
      Python/
        swan_compiler/
          lexer.py
          parser.py
          emitter.py
          sema.py
          cli.py
        tests/

Pipeline: .swan source -> Lexer -> Parser -> Syntax tree -> C emitter -> .c source.

## Tests

    PYTHONPATH=Compiler/Python python3 -m unittest discover -s Compiler/Python/tests -v

CI also checks that emitted C can be compiled and run. That is a test of the output, not a stage performed by the Swan compiler.

## Visual Studio Code

The vscode directory contains syntax highlighting, brace configuration, and snippets for Swan source files.
