# Compiler Performance

## Current pipeline

`.swan -> Lexer -> Parser -> AST -> Semantic analysis -> C source generation`

Swan emits C11. It does not invoke a C compiler as part of its own source-to-C compilation pipeline.

## Current optimization work

- Semantic analysis now stores the resolved type on every expression node. Code generation reads that annotation instead of repeating recursive type inference and scope/function lookups.
- The C generator no longer builds and searches duplicate scope maps solely to rediscover types already checked by semantic analysis.
- Non-entry Swan functions are emitted as `static inline` C functions. This gives the native C compiler explicit internal-linkage and inlining opportunities for small helpers.
- Swan `int` lowers to `int64_t`, matching the 64-bit integer model used by the native benchmark implementations and avoiding the previous mismatch against C/Rust/Go/Swift/Zig/Goose 64-bit loop variables.

## Measurement policy

No performance improvement is claimed without a reproducible baseline. Before optimizing:

1. Record compiler version, target platform, CPU, and build flags.
2. Use fixed small, medium, and large Swan source fixtures.
3. Measure lexing, parsing, semantic analysis, and code generation separately.
4. Record wall time, peak resident memory, and allocation counts where available.
5. Run repeated trials and report median and spread.
6. Verify generated program output before comparing timings.

The native cross-language benchmark measures end-to-end compile time (Swan-to-C translation plus native C compilation for Swan) and executable runtime separately. It must use equivalent integer widths and verify identical checksums before reporting timings.

Correctness and generated-code equivalence take precedence over compile-time improvements.
