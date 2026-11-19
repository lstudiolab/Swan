# Brew automatic memory management

Brew is a high-level language. Programmers work with ordinary values and
references; they do not manage allocation, deallocation, pointer lifetimes, or
ownership transfers as part of normal application code.

Brew uses automatic reference counting (ARC) as its default managed-memory
model. The compiler and runtime insert the retain/release operations needed to
keep referenced objects alive and reclaim them when they are no longer
reachable through managed references.

There is no raw-library mode that disables automatic memory management. Importing
a library must never silently change the memory-safety or lifetime rules of the
calling Brew code.

## Language contract

1. **Automatic allocation and cleanup.** Ordinary Brew code never needs to pair
   allocation with a manual free operation.
2. **Automatic reference counting.** Managed references keep their objects
   alive. The runtime reclaims an object when its managed reference count reaches
   zero.
3. **No manual ownership syntax.** Everyday Brew code does not require
   `own`, `borrow`, `move`, `drop`, `stack`, `heap`, or `arena`.
4. **No raw mode.** There is no import modifier, library annotation, or compiler
   switch that turns off ARC for a module or for its callers.
5. **Safe library boundaries.** Imported libraries are exposed through typed
   interfaces and managed wrappers. External resources must use wrapper-defined
   cleanup, such as closing a file or releasing a native handle; this does not
   disable ARC for Brew values.
6. **Compiler-controlled storage.** The compiler may use registers, stack slots,
   heap allocations, arenas, pooling, or other optimizations internally. These
   choices do not change Brew source semantics.
7. **Deterministic resource cleanup.** Objects and external resources must be
   released according to documented lifetime rules. ARC handles managed object
   memory; resource wrappers handle operating-system or library resources.
8. **Cycle handling.** The runtime must provide a defined strategy for reference
   cycles, such as compiler-supported weak references or cycle detection.
   Cycles must not silently cause unbounded leaks.

## What programmers write

Normal Brew code uses values and library APIs. The compiler handles object
lifetimes:

```brew
import std

func make_message() -> string:
    let message = "Hello from Brew"
    return message

func main() -> int:
    let message = make_message()
    print(message)
    return 0
```

There is no explicit allocation/free pair and no manual ownership transfer in
this example. The returned string remains alive while it is referenced and is
reclaimed automatically when its last managed reference is gone.

## Library and native interoperability

A normal Brew import always retains Brew's automatic memory-management rules.
A library cannot opt the importing source file out of ARC.

For libraries implemented outside Brew:

- Prefer typed Brew wrappers that expose ordinary Brew values.
- Copy or convert externally owned data into managed Brew values when needed.
- Wrap native handles in managed objects whose cleanup routine releases the
  external resource exactly once.
- Document whether a native function borrows data for the duration of a call,
  copies it, or returns a separately owned resource.
- Do not expose arbitrary pointer arithmetic or unmanaged memory as ordinary
  Brew application syntax.

A wrapper may internally call native allocation or release functions. That is
an implementation detail of the wrapper, not a mode change for the Brew
program. ARC does not automatically know how to release an arbitrary external
resource; the wrapper must define that resource's cleanup behavior.

## Compiler implementation requirements

The compiler/runtime must implement ARC consistently across all supported
targets:

1. Semantic analysis identifies managed reference values and their copy,
   assignment, return, capture, and container operations.
2. SIL or the typed intermediate representation records the lifetime
   operations needed by the compiler. These are internal implementation
   details, not source-level ownership keywords.
3. Compiler passes insert retain/release operations and eliminate redundant
   operations where safe.
4. Every control-flow exit, including early returns and error paths, releases
   references exactly as required.
5. Function calls and module boundaries use consistent managed-value
   conventions.
6. Native library wrappers define cleanup for external resources and prevent
   unmanaged handles from being mistaken for ordinary managed objects.
7. The runtime defines and tests its reference-cycle strategy.
8. Every backend implements equivalent observable lifetime behavior.

## Current implementation status

The current compiler does not yet provide complete ARC across the language.
The native x86-64 AST compatibility backend has limited compiler-generated
lifetime cleanup for selected array forms, including stack placement for small
fixed-size non-escaping arrays and conservative cleanup for eligible heap
arrays.

That implementation is not a complete ARC runtime. The typed SVM IR, SIL,
other backends, general strings, structs, closures, collections, and imported
library wrappers do not yet share a complete retain/release implementation.
Do not claim universal automatic memory management until the compiler/runtime
and cross-target tests establish it.

## Acceptance criteria

ARC is complete only when tests demonstrate:

- Ordinary values and references require no manual allocation or deallocation.
- Copies, assignments, returns, captures, and container storage preserve
  object lifetime.
- Objects are reclaimed after their final managed reference is released.
- Early returns, loops, branches, and error paths do not leak or double-release.
- Reference cycles follow the documented weak-reference or cycle-collection
  policy.
- Native resource wrappers release external resources exactly once.
- Importing any ordinary library never disables ARC.
- All supported targets pass lifetime, stress, sanitizer, and interoperability
  tests.

Brew aims to make ordinary application programming high-level and automatic.
Low-level allocation strategies remain compiler/runtime implementation details,
not a second programming mode that users must learn.
