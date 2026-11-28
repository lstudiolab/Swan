# swanc

`swanc` is the C++ frontend/compiler driver for Swan. It emits one C11 translation unit and delegates machine-code generation, object generation, linking, and target-specific ABI work to Clang.

## Build

```sh
cmake -S swanc -B build
cmake --build build
./build/swanc --emit-c build/swan.c path/to/main.swan
clang -std=c11 build/swan.c -o build/program
```

Use `--check` to lex and parse without emitting C. The current frontend is an intentionally early vertical slice: lexical analysis, a small function/statement parser, diagnostics, and C emission are implemented; type checking, imports, and complete language semantics are explicit next stages.

## Pipeline

1. Driver expands source-file arguments and owns compilation options.
2. Source manager loads files and assigns stable source IDs.
3. Lexer converts bytes to tokens, preserving source spans.
4. Parser builds an AST; no C text is generated during parsing.
5. Semantic analysis (next stage) resolves names, types, visibility, and import cycles.
6. Lowering (next stage) converts the typed AST to a target-independent, C-oriented IR.
7. Translation-unit emitter concatenates module declarations/definitions in deterministic order.
8. Driver invokes Clang (or leaves the generated C file for inspection).

## Memory model direction

Swan has no language-level garbage collector and no implicit general-purpose heap. Local values have lexical scope and stack storage; globals have static storage. Aggregate layout is explicit and predictable. Any future dynamic allocation must be an explicit, opt-in facility rather than hidden behavior. The compiler itself may use ordinary C++ implementation allocations; that is separate from Swan program semantics.

## Architecture notes

The separation resembles Swift's frontend stages (source management, lexer/parser, AST, semantic analysis, SIL-like lowering) and Objective-C's Clang-based compilation model (frontend emits C/LLVM-compatible representations; Clang owns target code generation and ABI details). Swan does not copy Swift or Objective-C language semantics or runtime.
