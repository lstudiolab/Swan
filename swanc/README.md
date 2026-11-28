# swanc

`swanc` is the C++ frontend/compiler driver for Swan. It emits one C11 translation unit and is designed to delegate machine-code generation, object generation, linking, and target-specific ABI work to Clang.

## Build

```sh
cmake -S swanc -B build
cmake --build build
./build/swanc --check examples
./build/swanc --emit-c build/swan.c examples
clang -std=c11 build/swan.c -o build/swan
```

Pass individual `.swan` files or a directory. Directory inputs are searched recursively and sorted before compilation. The generated C contains forward declarations so functions can refer to functions defined in other source files.

## Current implementation

- C++17 compiler executable and CMake build.
- Lexer recognizes the complete keyword list supplied for Swan, identifiers, numeric/string literals, comments, and basic punctuation/operators.
- Parser currently supports function declarations, primitive parameter/return types, blocks, local declarations, return, if/else, while, and basic expressions.
- Semantic pass currently checks duplicate functions/parameters and primitive signature types.
- C emitter produces one C translation unit for all supplied source files, with prototypes before definitions.
- `--check` runs frontend and semantic checks; `--emit-c` writes generated C.

This is a bootstrap compiler, not yet a complete implementation of the language. Function calls, imports, structs, enums, full expression/statement coverage, type inference, control-flow validation, and a dedicated IR/lowering pass remain to be implemented. In particular, the sample is a syntax roadmap and is not yet accepted in full because call expressions are not implemented.

## Planned pipeline

1. **Driver / project loader** — discover source files, normalize paths, establish compilation options.
2. **Source manager** — own source buffers and map spans to file/line/column.
3. **Lexer** — tokenize the source and recognize Swan keywords.
4. **Parser** — build an AST without emitting target code.
5. **Semantic analysis** — scopes, symbol resolution, type checking, mutability, visibility, and diagnostics.
6. **Lowering / Swan IR** — normalize control flow, resolve language sugar, and make storage/lifetime decisions explicit.
7. **Module linker** — resolve imports, detect cycles, deduplicate declarations, and construct one ordered program.
8. **C translation-unit emitter** — emit declarations, layouts, prototypes, and definitions into exactly one C file.
9. **Clang driver** — invoke Clang for C compilation and final linking.

## Memory model direction

Swan has no language-level garbage collector and no implicit general-purpose heap. Local values should have lexical scope and stack storage; globals should have static storage. Aggregate layout and ownership/lifetime rules must be explicit and statically checked. The compiler itself may use ordinary C++ implementation allocations; that is separate from Swan program semantics.

The language design must still specify how references, slices, escaping values, returned aggregates, and resource cleanup work. Swift-style ownership/lifetime analysis is a useful architectural reference, but Swan should not inherit Swift ARC or a runtime heap by accident.

## Architecture references

- **Swift:** source management, lexer/parser, AST, semantic analysis, and a dedicated lowering representation (SIL) are separated into stages. Swan should follow the separation, not copy Swift's runtime or ARC.
- **Objective-C / Clang:** Clang owns parsing/code generation for C-family targets and platform ABI details. Swan emits C as a stable backend boundary, then Clang produces native code. Objective-C runtime features are not part of Swan unless deliberately designed later.
