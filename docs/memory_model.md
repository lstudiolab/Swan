# Swan compiler-managed memory model

Swan uses a compiler-managed lifetime model inspired by Goose. The programmer
works with values; the compiler determines their storage duration and emits
allocation and reclamation code. Ordinary Swan code must not require explicit
allocation/free pairs, a garbage collector, or per-object reference counters.

This document distinguishes the current implementation from the target design.
A design goal is not an implemented guarantee.

## Design contract

1. **No routine manual deallocation.** There is no supported source-level
   free(value) or manual allocation modifier. Compiler-generated calls to
   the platform allocator are implementation details, not language operations.
2. **No tracing garbage collector.** Swan does not pause execution to discover
   unreachable objects.
3. **No reference counting.** Ordinary ownership does not increment or decrement
   runtime counters on every copy or reference.
4. **Static lifetime analysis.** The compiler tracks definitions, uses, control
   flow, aliases, escapes, and ownership transfers before selecting storage.
5. **Stack first.** Values with statically bounded size and a lifetime contained
   by the current activation or lexical scope should use registers or stack
   storage.
6. **Deterministic cleanup.** Heap-backed values are released at their proven
   last use or scope exit. Cleanup must cover early returns and all control-flow
   exits, and must not double-release a value.
7. **Escape-safe ownership.** A value that may outlive its current scope must
   be transferred to an owner whose lifetime is sufficient. If the compiler
   cannot prove a safe transfer, it must preserve the allocation or reject the
   operation; it must never guess and free early.
8. **Explicit low-level control remains possible.** Unsafe pointer operations
   and allocator/FFI integration belong in clearly marked low-level interfaces,
   not in the ordinary value-lifetime path.

## Current implementation

The native x86-64 AST compatibility backend has a limited automatic lifetime
implementation for array, array_u32, and their unchecked variants:

- Small, fixed-size, non-escaping arrays can be placed in the function stack
  frame.
- Dynamic or larger arrays use heap storage.
- Eligible heap arrays are released after their last recognized use.
- Cleanup is emitted for early returns and normal function exit.
- Handles are initialized so paths that skip a declaration do not release
  uninitialized storage.
- Escape analysis conservatively avoids releasing arrays passed to calls or
  returned from a function when the backend cannot prove a safe transfer.
- --no-heap rejects array allocations that cannot be proven to fit the
  current bounded stack-storage path.

This is automatic management: users do not pair allocation calls with
source-level free calls. The emitted free@PLT call is compiler-generated
runtime cleanup.

## Current limitations

The current implementation is **not yet Goose-equivalent across the language**:

- Lifetime analysis is specific to arrays in the native x86-64 AST backend.
- SIL has ownership markers, but does not yet carry a complete, verified
  ownership dataflow model or drive SVM lowering.
- The typed SVM IR and non-x86 backends do not yet share the x86-64 array
  cleanup implementation.
- General strings, structs, enums with payloads, closures, dynamic collections,
  and arbitrary owned aggregates do not yet have a unified ownership model.
- Interprocedural escape summaries, explicit borrowing rules, move checking,
  and safe ownership transfer are incomplete.
- Exception unwinding and guaranteed cleanup for all future error paths are not
  implemented.
- Imported C code and operating-system APIs can allocate outside Swan analysis.
  FFI contracts must describe who owns returned memory and how it is released.
- --no-heap only constrains the currently supported Swan array allocations;
  it cannot constrain allocations performed inside imported native libraries.

## Required compiler architecture

The complete model must be implemented as a target-independent compiler
analysis, not as a collection of backend-specific cleanup heuristics.

1. Semantic analysis establishes value types, ownership categories, mutability,
   and legal borrow/move operations.
2. SIL represents owned values, guaranteed borrows, address-only values,
   ownership transfer, destruction, and lexical scope boundaries.
3. SIL verification performs control-flow-aware ownership checking: every owned
   value has one valid destruction/transfer path, borrows do not outlive their
   owner, and no value is used after destruction.
4. Interprocedural summaries describe which parameters are borrowed, consumed,
   or allowed to escape, including imported-function annotations.
5. A target-independent lifetime pass computes last-use points and storage
   classes: register, stack, bounded arena, or heap.
6. Cleanup insertion handles normal exits, early returns, loop exits, and
   exceptional exits before lowering to SVM IR.
7. Every target backend lowers the same verified ownership operations. Backend
   differences must not change source-level lifetime semantics.

## Acceptance criteria

The model is not complete until tests demonstrate all of the following:

- No user-written deallocation is needed for ordinary values.
- No runtime reference counts or tracing collector are emitted.
- Stack promotion works for bounded non-escaping aggregates on every supported
  backend where stack storage is available.
- Heap values are reclaimed at last use or the end of their proven owner scope.
- Moves, borrows, aliases, returns, captures, and calls preserve lifetime safety.
- Nested branches, loops, break, continue, early returns, and error exits
  produce exactly-once cleanup.
- Invalid use-after-move, use-after-destroy, and escaping-borrow programs are
  rejected with source locations.
- Sanitizer tests, allocation counters, stress tests, and cross-target runtime
  tests validate the generated behavior.

Swan is not currently a memory-safe or safety-certified language. Compiler
analysis cannot make arbitrary unsafe pointer arithmetic, data races, foreign
code, or undefined behavior safe by rewriting generated machine code.
