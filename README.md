# Swan

Swan is a compiled programming language with a native C++20 compiler and a direct x86-64 assembly backend.

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

Use consistent indentation (four spaces are recommended). Increasing indentation opens a block; decreasing indentation closes blocks. No closing brace or end keyword is needed.

Functions may declare typed parameters and return types:

    fn add(left: int, right: int) -> int:
        return left + right

An unannotated function with a value-returning body currently infers an integer return type; a function without a value return defaults to void.

## Build and run

Build Swan, then compile and run a source file:

    sh build.sh
    ./build/swan examples/hello.swan

Build a reusable native executable:

    ./build/swan build examples/hello.swan -o hello
    ./hello

The explicit run command is equivalent:

    ./build/swan run examples/hello.swan

Emit assembly for inspection:

    ./build/swan examples/hello.swan --emit-asm -o hello.s

Swan emits GNU assembler syntax for x86-64 Linux (System V ELF), then invokes the configured assembler/linker driver (CC, default `cc`). It does not generate C source or invoke a C compiler as an intermediate code-generation stage.

## Implemented frontend and backend

- Lexer with source locations, indentation tokens, comments, strings, numbers, and operators.
- Recursive-descent parser and precedence-based expression parser.
- Semantic analysis with resolved expression types.
- Direct x86-64 assembly generation for functions, scalar locals, parameters, calls, arithmetic, comparisons, boolean operations, branches, loops, loop control, strings, and console logging.
- Native executable linking through the system toolchain.

The initial native backend targets x86-64 Linux. Other target architectures and advanced aggregate lowering are not yet implemented. Struct declarations are type-checked; struct member access remains unsupported.

Recognizing a keyword does not mean its semantics are complete. Imports, for, loop, async operations, and the ownership/recovery semantics of hold, store, operate, and clone are not implemented yet.

## Tests

    sh test.sh

See docs/ARCHITECTURE.md, docs/COMPILER_AUDIT.md, docs/PERFORMANCE.md, docs/TESTING.md, and docs/ROADMAP.md for engineering details.