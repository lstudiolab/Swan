# Compiler Performance

## Current pipeline

`.swan -> Lexer -> Parser -> AST -> Semantic analysis -> C source generation`

Swan currently emits C11. It does not invoke a C compiler as part of its own compilation pipeline.

## Hot paths to profile

### 1. Lexing

The scanner visits each source character and creates token values. Token kind and token value are still stored as strings, so token representation remains a larger future optimization. The current pass uses binary search for keyword classification, avoids constructing a temporary two-character string on punctuation checks, scans identifiers and numeric literals directly from the source, and caps the initial token-vector reservation.

Potential future improvements:

- Replace token-kind strings with a compact enum.
- Store source offsets and lengths for identifier and numeric tokens instead of eagerly copying text.
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

The C emitter recursively constructs expression strings. Semantic analysis now records each expression's resolved built-in type in a compact enum on the AST. Code generation consumes that result rather than recursively inferring the type again. Non-built-in types retain the existing fallback path. Generated C is appended to a reserved `std::string` buffer instead of an `ostringstream`, avoiding stream-formatting overhead and the final stream-to-string copy.

Potential future improvements:

- Reduce temporary strings created while assembling individual expressions and statements.
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

The benchmark suite currently has two distinct workloads: a deterministic 2,500-declaration compiler workload and an equivalent 50-million-iteration scalar program compiled by Swan, Rust, and Goose. The latter checks output equality and reports compile-pipeline time, runtime, and peak RSS. Neither substitutes for phase-level compiler profiling.

## Optimization order

1. Profile and reduce lexer allocations.
2. Measure AST allocation cost before introducing an arena.
3. Avoid repeated semantic type resolution in code generation.
4. Add symbol IDs if scope lookup is measurable.
5. Re-run the same fixtures after every optimization.

Correctness and generated-code equivalence take precedence over compile-time improvements.

## Implemented low-risk optimizations

- Keyword classification uses binary search over a sorted, compile-time keyword table.
- Punctuation scanning checks operator characters directly and avoids a temporary two-character string.
- The lexer scans identifiers and numeric literals directly from the source and constructs each token value once.
- Initial token-vector reservation is capped to avoid oversized allocations for whitespace-heavy or unusually large source files.
- Parser token access returns references to avoid copying complete token objects.
- Program parsing reserves declaration storage; code generation reserves structure/function indexes and uses a hash map for struct dependency lookup.
- Semantic analysis stores built-in expression types in a compact enum. Code generation reuses the cached result instead of recursively inferring types again.
- C output uses a reserved string buffer and appends indentation directly, avoiding stream formatting and a final buffer copy.

These changes reduce known repeated work and avoid selected allocations. They are not a claim of a measured percentage improvement. Use the benchmark harness on the same machine before and after changes to quantify end-to-end impact.
