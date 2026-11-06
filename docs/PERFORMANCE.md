# Compiler Performance

## Current pipeline

`.swan -> Lexer -> Parser -> AST -> Semantic analysis -> C source generation`

Swan currently emits C11. It does not invoke a C compiler as part of its own compilation pipeline.

## Hot paths to profile

### 1. Lexing

The scanner visits each source character and creates token values. Token kind and token value are currently stored as strings. Identifier, numeric, and string scanning can therefore create repeated string work.

Potential future improvements:

- Replace token-kind strings with a compact enum.
- Store source offsets and lengths for identifier and numeric tokens instead of eagerly copying text.
- Reserve token storage when a useful estimate is available.
- Keep decoded string-literal storage only where escape processing requires it.

### 2. Parsing and AST allocation

Expression parsing is recursive and AST nodes use shared ownership. Shared pointers are convenient for the current small compiler, but reference-counting and per-node allocations may become measurable on large source files.

Potential future improvements:

- Measure allocation counts and peak memory.
- Consider a compilation-unit AST arena if allocation overhead is demonstrated.
- Preserve the current parser precedence model unless profiling identifies a real issue.

### 3. Semantic analysis

Scope lookup currently searches nested unordered maps by source-level identifier. Deeply nested scopes and repeated identifier references can increase lookup work.

Potential future improvements:

- Assign stable symbol IDs during declaration analysis.
- Resolve each identifier once and retain its symbol reference.
- Represent types with compact IDs rather than repeated strings.
- Preserve source names for diagnostics.

### 4. C generation

The C emitter recursively constructs expression strings. Its inferred-type routine walks expression nodes again, repeating some work already performed during semantic analysis.

Potential future improvements:

- Store resolved expression types on a typed AST or semantic side table.
- Emit into a shared output buffer instead of returning many intermediate strings.
- Reuse resolved symbol and type information.
- Measure generated output size and emission time independently.

## Measurement policy

No performance improvement is claimed without a reproducible baseline. Before optimizing:

1. Record compiler version, target platform, CPU, and build flags.
2. Use fixed small, medium, and large Swan source fixtures.
3. Measure lexing, parsing, semantic analysis, and code generation separately.
4. Record wall time, peak resident memory, and allocation counts where available.
5. Run repeated trials and report median and spread.
6. Verify generated program output before comparing timings.

The first benchmark suite should include identifier-heavy input, deeply nested expressions, many declarations, many function calls, and large string literals.

## Optimization order

1. Profile and reduce lexer allocations.
2. Measure AST allocation cost before introducing an arena.
3. Avoid repeated semantic type resolution in code generation.
4. Add symbol IDs if scope lookup is measurable.
5. Re-run the same fixtures after every optimization.

Correctness and generated-code equivalence take precedence over compile-time improvements.


## Implemented low-risk optimizations

The current optimization pass makes five targeted changes:

- Keyword classification uses binary search over a sorted, compile-time keyword table instead of a linear scan.
- Punctuation scanning checks operator characters directly and only constructs a string for a recognized token, avoiding the temporary two-character string on the punctuation path.
- The lexer reserves an initial token capacity based on source length to reduce repeated vector growth on ordinary source files.
- Expression type inference evaluates each binary operand's inferred type once. The previous implementation could recursively infer the left subtree twice and the right subtree once, multiplying work on left-heavy expression trees.
- C output indentation is written directly to the stream instead of allocating a temporary indentation string for every emitted line.

These are algorithmic/allocation reductions, not a claim of a measured percentage improvement. Run the benchmark harness on the same machine before and after changes to quantify end-to-end impact. The harness measures a deterministic 2,500-declaration file; phase-level profiling and peak-memory measurement remain future work.
