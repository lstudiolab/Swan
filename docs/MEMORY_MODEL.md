# Swan Memory Model

## Current guarantees

Swan does not require developers to write a manual cleanup block for ordinary local arrays.

The native x86-64 AST backend currently performs storage and lifetime analysis for
`array` / `array_u32` values:

- Small, fixed-size arrays that do not escape and are not manually freed can be
  placed directly in the function's stack frame.
- Arrays that require dynamic storage use the heap.
- Compiler-inserted cleanup releases eligible heap arrays after their final use.
- Cleanup is emitted on early-return paths as well as normal function exit.
- Values detected as escaping are not released prematurely.
- Explicit `free(array_name)` remains available for code that deliberately manages
  an array's lifetime.

The compiler has regression coverage for stack promotion, last-use cleanup,
early-return cleanup, branch-local initialization, and escaped arrays.

## What is not guaranteed yet

- Swan does not have a complete borrow checker or general ownership type system.
- Arbitrary pointers and owned general-purpose strings are not yet native Swan
  value types.
- Dynamic arrays can allocate from the heap. Swan is not a zero-heap language.
- The automatic lifetime analysis described above is currently specific to the
  native x86-64 compatibility backend; it is not a cross-target ownership model.
- The compiler cannot safely repair arbitrary use-after-free, data races, invalid
  pointer arithmetic, or undefined behavior by rewriting generated assembly.

## Design direction

The compiler should own ordinary value lifetimes by default. The planned memory
model is:

1. Prefer registers and stack storage for bounded, non-escaping values.
2. Use bounded arenas or explicit allocator domains for long-lived and dynamic
   data structures.
3. Make ownership transfer and borrowing explicit in semantic analysis.
4. Insert deterministic cleanup on every control-flow exit.
5. Reject programs whose lifetime or aliasing rules cannot be proven safe.
6. Offer a no-heap profile only when every allocation can be statically bounded;
   it must diagnose unsupported dynamic allocation rather than silently falling
   back to the heap.

Aviation, banking, medical, and other safety-critical deployments require a
separate qualification effort: deterministic allocation, concurrency and
atomicity guarantees, a documented subset, static analysis, reproducible builds,
and target-specific verification. The current compiler is not safety-certified.