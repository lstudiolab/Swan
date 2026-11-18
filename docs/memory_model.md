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

## Raw mode and the i64 interface

Importing `raw` opts the whole program into the raw execution model. Without
that import, Swan's existing automatic array lifetime behavior is unchanged.
With `raw`, compiler-inserted array reclamation and automatic array stack
promotion are disabled. Raw programs must manage raw allocations explicitly.

### Address representation

Raw addresses and byte counts use `i64`. On the implemented x86-64 System V
backend, an address is carried in a 64-bit general-purpose register. This is
a machine-word ABI convention, not a portable pointer type. The compiler
rejects raw mode on other target architectures until they have matching
pointer, memory-operation, and lifetime implementations.

### Raw allocation interface

| Operation | Signature | Behavior |
| --- | --- | --- |
| Heap allocation | `raw.alloc(size: i64) -> i64` | Allocate uninitialized bytes |
| Zeroed allocation | `raw.alloc_zeroed(count: i64, size: i64) -> i64` | Allocate zeroed bytes |
| Heap alias | `raw.heap(size: i64) -> i64` | Allocate uninitialized heap bytes |
| Resize | `raw.resize(pointer: i64, size: i64) -> i64` | Resize; preserves the old pointer on allocation failure |
| Release | `raw.release(pointer: i64)` | Release an unmanaged raw pointer |
| Copy | `raw.copy(dst: i64, src: i64, size: i64) -> i64` | Copy non-overlapping bytes |
| Move bytes | `raw.move_bytes(dst: i64, src: i64, size: i64) -> i64` | Copy with overlap support |
| Fill | `raw.fill(dst: i64, value: i64, size: i64) -> i64` | Fill a byte range |
| Stack allocation | `raw.stack(size: i64) -> i64` | Dynamically reserve aligned stack bytes until function return |
| Arena creation | `raw.arena(capacity: i64) -> i64` | Create a fixed-capacity heap-backed bump arena |
| Arena allocation | `raw.arena_alloc(arena: i64, size: i64) -> i64` | Allocate an aligned slice from an arena |
| Arena reset | `raw.arena_reset(arena: i64)` | Rewind the arena to offset zero |
| Arena remaining | `raw.arena_remaining(arena: i64) -> i64` | Return remaining capacity |
| Arena release | `raw.arena_release(arena: i64)` | Release the arena backing allocation; transfer an owned arena with `move` |

Allocation functions return zero on failure. Negative sizes are rejected by the
Swan wrappers. The arena stores capacity and used-byte offsets in a 16-byte
header and aligns allocations to 8 bytes. Arena slices remain valid until the
arena is reset or released. Declare an arena slice with `borrow let`; its
lexical borrow prevents resetting, moving, or releasing the arena until the
slice leaves scope. `raw.stack` allocations are function-scoped, are rejected
inside loops to prevent unbounded stack growth, and cannot be returned through
tracked local aliases.

### Direct memory operations

The following functions are compiler intrinsics, not ordinary library calls:

- `raw.load8/16/32/64(address: i64) -> i64` — unsigned load from the exact
  address.
- `raw.store8/16/32/64(address: i64, value: i64)` — store the low 8, 16, 32,
  or 64 bits at the exact address.

On x86-64 these lower to direct `mov` instructions with byte, word, dword, or
qword memory operands. They do not perform bounds checks, alignment checks,
or pointer-provenance checks. Invalid addresses and out-of-bounds access have
native undefined behavior.

### Ownership syntax

Raw owner and borrow declarations are checked by semantic analysis:

```swan
import raw

func main() -> int:
    own var buffer: i64 = raw.heap(64)

    if raw.load8(borrow buffer) == 0:
        borrow let view: i64 = buffer
        raw.store8(view, 65)

    own var resized: i64 = raw.resize(move buffer, 128)
    drop resized
    return 0
```

- `own var name: i64 = allocation` creates the unique owner of an allocation.
- `borrow let view: i64 = owner` creates an immutable, lexical view. A view
  cannot be copied into an ordinary variable or returned.
- `borrow owner` creates a temporary borrow for a call argument.
- `borrow let slice: i64 = raw.arena_alloc(borrow region, size)` creates a
  view whose lifetime is tied to the arena owner. The arena cannot be reset,
  moved, or dropped while that view remains in scope.
- Borrowed pointers may only be passed to the raw module's non-retaining
  memory operations. Passing one to an ordinary Swan function or external
  function is rejected because the compiler cannot prove that the callee
  does not retain or return the address. Scalar reads such as `raw.load32`
  remain usable in expressions.
- `move owner` transfers ownership and invalidates the previous owner.
- `drop owner` releases an owner exactly once.
- `raw.stack(size)` allocates function-lifetime stack storage. It is rejected
  inside loops because repeated dynamic stack allocation would accumulate
  until the function returns. Allocate before the loop or use an arena.

The semantic checker rejects use after move/drop, copying an owner, dropping or
moving an owner while a lexical borrow is live, and leaving a scope or
returning while an owner remains live. Branches must agree on each owner's
state; ownership cannot be changed inside a loop when that would make its
post-loop state ambiguous.

`raw.release` remains available for unmanaged `i64` pointers and internal
runtime code. For values declared with `own`, prefer `drop` so the compiler
can track the owner state; `raw.release(move owner)` and
`raw.arena_release(move arena)` are also consuming operations. A borrowed view cannot be passed to `raw.release`.

### Safety boundary

Raw mode is deliberately unsafe. The current implementation checks lexical
ownership of variables declared with `own`, but it cannot prove that arbitrary
`i64` values are valid addresses, that a foreign function does not retain a
borrow, or that pointer arithmetic stays within an allocation. A raw pointer
stored in an ordinary `i64` is outside the owner checker. The programmer is
responsible for FFI contracts, bounds, alignment, aliasing, synchronization,
and exactly-once release.

The default Swan memory model remains separate: ordinary values continue to
use the compiler's existing automatic behavior and do not require `own`,
`borrow`, `move`, or `drop`.
