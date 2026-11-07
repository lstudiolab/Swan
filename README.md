# Swan

Swan is a programming language with a native C++20 compiler. It currently parses and validates Swan source, then emits portable C11.

## Language syntax

Swan uses indentation to define code blocks. Curly braces are not block delimiters. Function declarations end with a colon; control-flow statements start their indented body on the next line.

    fn main():
        log "Hello, Swan!"
        return 0

    if true
        log "Condition is true"
    else
        log "Condition is false"

    while false
        log "Running"

Use consistent indentation (four spaces are recommended). Increasing indentation opens a block; decreasing indentation closes blocks. A single decrease can close several nested blocks at once: the lexer emits one dedent token for every indentation level crossed, and the parser consumes those tokens as enclosing blocks return. No closing brace or end keyword is needed.

Functions may declare typed parameters and return types, for example: fn add(left: int, right: int) -> int:

An unannotated function with a value-returning body currently infers an integer return type; a function without a value return defaults to void.

## Build and run

Build the Swan compiler, then run a source file directly:

    sh build.sh
    ./build/swan examples/hello.swan

The compiler recognizes `.swan` files, emits C11 internally, invokes the configured C compiler, and runs the resulting native executable. Set `CC` to select the C compiler (defaults to `cc`).

Build a reusable native executable without running it:

    ./build/swan build examples/hello.swan -o hello
    ./hello

The explicit run command is equivalent to the shorthand:

    ./build/swan run examples/hello.swan

To inspect or save generated C instead:

    ./build/swan examples/hello.swan --emit-c -o hello.c

Requirements: a C++20 compiler to build Swan and a C11 compiler (normally Clang or GCC) to compile Swan programs. Python and CMake are not required.

## Implemented frontend

- Lexer with source locations, indentation tokens, comments, strings, numbers, and operators.
- Recursive-descent parser and precedence-based expression parser.
- AST, semantic analysis, and C11 generation.
- Functions, typed parameters, indented structs, let/var/const, if/else, while, return, loop control, arithmetic, comparisons, boolean operations, calls, and log.

Recognizing a keyword does not mean its semantics are complete. Imports, for, loop, async operations, and the ownership/recovery semantics of hold, store, operate, and clone are not implemented yet.

## Tests

    sh test.sh

See docs/ARCHITECTURE.md, docs/COMPILER_AUDIT.md, docs/PERFORMANCE.md, docs/TESTING.md, and docs/ROADMAP.md for engineering details.
