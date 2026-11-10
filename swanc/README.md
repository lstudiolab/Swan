# swanc

`swanc` is Compiler 2, the Swan implementation of the Swan compiler.

Compiler 1 remains in `compiler/` as the C bootstrap compiler. It is intentionally preserved until Compiler 2 is self-hosting, stable, and explicitly approved for removal.

## Current implementation order

1. Lexer: scan UTF-8 source into tokens with source locations.
2. Parser: consume tokens and build the AST.
3. AST: define the node model shared by later compiler stages.

The lexer and parser each remain a single main source file. Other subsystems will be introduced as focused files inside their own directories when their real implementation is needed. Avoid speculative placeholder modules and giant files.

## Bootstrap

Compiler 1 builds `swanc`; `swanc` then builds itself. Self-hosting is only considered proven after the second-stage compiler can rebuild the same source and pass the compiler regression suite.

## Compiler constraints

- Compile source to native/target code; do not add an interpreter execution path.
- Keep the intermediate representation target-independent.
- Keep target-specific lowering and code generation separate from parsing.
- Reject unsupported or invalid constructs with diagnostics; never silently skip them.
- Preserve Compiler 1 until explicit authorization is given to remove it.
