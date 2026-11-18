# Swan memory model

Swan keeps its existing compiler-managed memory behavior by default. Importing
`raw` opts the whole program into raw mode. Raw mode disables the native
x86-64 AST backend's compiler-inserted array reclamation; it does not change
programs that do not import `raw`.

## Default mode

Without `import raw`, existing array allocation, stack promotion, escape
analysis, and compiler-generated cleanup remain enabled. The current
implementation is intentionally limited to the native x86-64 AST backend;
this is not a claim of complete Goose-equivalent ownership on every target.

## Raw mode

`import raw` enables an explicit low-level interface. The `raw` module
provides:

| Function | Operation |
| --- | --- |
| `raw.alloc(size: i64) -> i64` | Allocate uninitialized bytes |
| `raw.alloc_zeroed(count: i64, size: i64) -> i64` | Allocate zeroed bytes |
| `raw.resize(pointer: i64, size: i64) -> i64` | Reallocate an allocation |
| `raw.release(pointer: i64)` | Release an allocation |
| `raw.copy(dst: i64, src: i64, size: i64) -> i64` | Copy non-overlapping bytes |
| `raw.move_bytes(dst: i64, src: i64, size: i64) -> i64` | Copy bytes with overlap support |
| `raw.fill(dst: i64, value: i64, size: i64) -> i64` | Fill a byte range |

These operations are native C allocator and memory functions. Allocation
failure follows the C ABI: allocation functions return zero/null. The caller
must check the result, release each live allocation exactly once, and must not
use an allocation after release or after a successful resize that moved it.

## Integer and address representation

`i64` is Swan's signed 64-bit integer type. On the currently implemented
x86-64 System V ABI, raw addresses are represented as `i64` machine words.
The same source-level spelling must not be interpreted as a portable pointer
representation on targets whose pointer width or ABI differs. The raw module
currently relies on the native C ABI and is intended for native x86-64 builds.

The compiler currently accepts named type annotations broadly; the language
does not yet perform complete static type checking or narrowing conversions.
In particular, `i64` annotations select the documented 64-bit interface but
do not yet provide Rust-style pointer provenance or bounds safety.

## Ownership keywords and future extensions

The raw allocator API is usable now. Dedicated `own`, `borrow`, `move`,
`drop`, `stack`, `heap`, and `arena` language constructs, plus direct
typed load/store intrinsics, require parser, semantic, verifier, and backend
support and are not claimed by this initial raw-mode implementation.
