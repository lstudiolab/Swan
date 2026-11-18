# Swan High-Level Language Contract

**Status: Normative language direction; compiler implementation is in progress.**

Swan is a high-level, general-purpose programming language. Its source language is designed around values, abstractions, and developer intent. Hardware details, memory layout, register allocation, object lifetimes, and target-specific instructions belong to the compiler and runtime—not ordinary Swan programs.

This document supersedes any earlier proposal that exposed manual memory management or raw hardware control as ordinary Swan language features.

## 1. Core principles

1. **Automatic memory management:** Swan manages the lifetime of ordinary values and objects. Application code does not allocate and free memory manually.
2. **Safe by default:** ordinary imports, libraries, and modules preserve the language's safety and memory rules. A dependency cannot silently disable them.
3. **Readable syntax:** common programs should require few annotations and little boilerplate.
4. **Type inference:** infer local types from expressions; allow explicit annotations where they improve clarity or are needed at boundaries.
5. **Useful abstractions:** strings, collections, functions, structures, enums, modules, errors, and concurrency should be first-class language/library concepts.
6. **Portable semantics:** a program should behave consistently across supported targets. Platform-specific capabilities are exposed through libraries and explicit platform modules.
7. **Native compilation:** Swan may compile ahead of time to native code or portable target formats. Compiler internals are not source-language concepts.

## 2. Not ordinary Swan syntax

The following are not part of the ordinary high-level language and must not be required in application code:

- Manual ownership and borrowing declarations such as `own`, `borrow`, and `move`.
- Explicit object destruction such as `drop`.
- User-directed storage placement such as `stack`, `heap`, and `arena`.
- Raw mode or an import modifier that disables automatic memory management.
- Pointer arithmetic, exposed addresses, register names, or machine instructions as general-purpose language constructs.
- Mandatory fixed-width integer annotations for everyday variables.

These words must not be advertised as supported high-level language keywords. If compatibility requires recognizing an old spelling, it must be treated as a deprecated syntax error or ordinary identifier according to the migration policy—not as a hidden low-level execution mode.

## 3. Memory model

Swan's ordinary managed-value model is automatic reference counting (ARC), with compiler/runtime lifetime analysis and optimizations. The implementation may use stack slots, registers, heap allocation, pooling, arenas, or other strategies internally when those strategies preserve observable Swan semantics.

The compiler/runtime is responsible for:

- Retaining values when references are created or copied.
- Releasing values when their managed lifetime ends.
- Correct cleanup on returns, branches, loops, and error paths.
- Safe behavior across function, module, and library boundaries.
- A defined policy for reference cycles (for example, weak references and/or cycle detection).
- Wrappers that deterministically release external resources such as files and native handles.

ARC does not itself close files, sockets, or operating-system handles. High-level resource APIs must provide deterministic cleanup abstractions without exposing manual memory management as the normal object-lifetime model.

**Implementation status:** ARC is not yet complete across Swan's types and targets. The compiler completion matrix remains authoritative for what is actually implemented. Do not claim universal ARC until the acceptance tests pass.

## 4. Types

Everyday code should use inferred or friendly semantic types:

- `int` for ordinary integers.
- `float` for floating-point values.
- `bool` for booleans.
- `string` for text.
- `list<T>`, `map<K, V>`, and `set<T>` for collections.
- User-defined structures and enums for domain models.

Fixed-width types such as `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, and `u64` may remain available for binary formats, FFI, networking protocols, and other explicit interoperability boundaries. They are not the default style for ordinary application code.

## 5. Canonical source style

The following is the intended high-level style. Examples describe the language direction; individual constructs must not be considered implemented until listed as complete in the compiler completion matrix.

```swan
import std

func average(values: list<int>) -> float:
    if values.is_empty():
        return 0.0

    return values.sum() / values.count()

let names = ["Ada", "Grace", "Katherine"]

for name in names:
    print("Hello, " + name)

print(average([10, 20, 30]))
```

The goal is that developers work with strings, lists, functions, and values. They should not need to know where those values are stored or when their memory is released.

## 6. Language feature direction

| Area | Intended high-level experience |
|---|---|
| Variables | Inferred `let`, mutable `var`, constants |
| Functions | `func`, inferred local types, clear signatures |
| Control flow | `if`, `else`, `for`, `while`, `match` |
| Data modeling | Structures, enums, properties, methods |
| Collections | Lists, maps, sets, ranges, iteration |
| Text | Managed strings and Unicode-aware operations |
| Errors | Typed/structured errors with clear propagation and handling |
| Concurrency | Tasks, async/await, safe synchronization |
| Modules | Imports, exports, package/dependency management |
| Interoperability | Typed FFI wrappers and explicit fixed-width boundary types |
| Portability | Consistent semantics across supported native and portable targets |

This table is a design target, not an implementation checklist. See `docs/compiler_completion_matrix.md` for verified implementation status.

## 7. Compiler architecture remains internal

Swan may continue to use typed IR, SIL, SVM, LLVM interoperability, native backends, ABI lowering, register allocation, and machine-code generation internally. These are compiler engineering mechanisms. They do not justify exposing manual memory management or hardware-oriented syntax to ordinary Swan developers.

The compiler can optimize storage and execution aggressively, but optimizations must preserve the language's defined behavior. The source language must not become low-level merely because its compiler has low-level components.

## 8. Compatibility and migration

- New examples and documentation must use high-level Swan syntax.
- Remove manual-memory and raw-mode syntax from the public language surface.
- Keep fixed-width types only where explicit representation matters.
- Do not silently reinterpret old ownership keywords as active memory-management controls.
- Update parser, semantic analysis, IR lowering, standard library, backends, diagnostics, tests, and editor tooling together before marking a feature complete.
- The completion matrix must distinguish the new language contract from features that have not yet been implemented.

## 9. Definition of done

The high-level direction is fully implemented only when:

1. Ordinary programs compile without manual memory-management syntax.
2. ARC works for all supported managed values and all supported targets.
3. Strings and general collections have complete managed semantics.
4. Structs, enums, methods, and type inference work end to end.
5. Errors clean up managed values on every exit path.
6. Standard library and module boundaries preserve managed-value semantics.
7. Cross-target conformance tests demonstrate equivalent observable behavior.
8. Old low-level source keywords are removed from the public grammar or produce deliberate migration diagnostics.
9. Documentation, compiler completion matrix, tests, and editor support agree.

Until these gates pass, Swan's high-level design is established but its implementation remains in progress.
