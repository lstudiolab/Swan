# Compiler Performance

## Current pipeline

`.swan -> Lexer -> Parser -> AST -> Semantic analysis -> C source generation`

Swan currently emits C11. It does not invoke a C compiler as part of its own compilation pipeline.

## Hot paths to profile

### 1. Lexing

The scanner visits each source character and creates token values. Token kinds are non-owning views into canonical static spellings, so they do not allocate or duplicate keyword and punctuation strings. Identifier and numeric lexemes are scanned directly from the source and copied once into owned token values; the parser later moves those values into the AST. The scanner uses binary search for keyword classification, avoids temporary two-character strings for punctuation, and caps its initial token-vector reservation.

Potential future improvements:

- Consider compact token-kind IDs if comparisons remain measurable after removing token-kind allocations.
- Store source offsets and lengths for identifier and numeric tokens if source-lifetime ownership can be made explicit.
- Keep decoded string-literal storage only where escape processing requires it.

### 2. Parsing and AST allocation

Expression parsing is recursive and AST nodes use shared ownership. AST nodes now retain only a compact `SourceLocation` rather than copying a complete token (including its kind and lexeme strings) into every expression, statement, parameter, and field.

Potential future improvements:

- Measure allocation counts and peak memory.
- Consider a compilation-unit AST arena if node allocation overhead is demonstrated.
- Preserve the current parser precedence model unless profiling identifies a real issue.

### 3. Semantic analysis

Scope lookup uses a single active-symbol hash index keyed by non-owning views of stable AST names. Entering a nested scope records the previous binding; leaving it restores shadowed symbols. This avoids walking every enclosing scope for each identifier lookup.

Potential future improvements:

- Assign stable symbol IDs during declaration analysis if hash lookup remains measurable.
- Resolve each identifier once and retain its symbol reference.
- Represent user-defined types with compact IDs rather than repeated strings.
- Preserve source names for diagnostics.

### 4. C generation

The C emitter appends expression fragments into one string per expression tree and writes statements, declarations, and function signatures directly into the final output buffer. It avoids recursively allocating intermediate expression strings and line-sized concatenation chains. String literal escaping appends directly into the final expression buffer rather than allocating an intermediate string or using an `ostringstream`. Semantic analysis records built-in expression types in a compact enum, and code generation reuses that metadata.

Potential future improvements:

- Reuse resolved symbol and type information throughout the backend.
- Measure generated output size and emission time independently.

## Measurement policy

No performance improvement is claimed without a reproducible baseline. Before optimizing:

1. Record compiler version, target platform, CPU, and build flags.
2. Use fixed small, medium, and large Swan source fixtures.
3. Measure lexing, parsing, semantic analysis, and code generation separately.
4. Record wall time, peak resident memory, and allocation counts where available.
5. Run repeated trials and report median and spread.
6. Verify generated program output before comparing timings.

The compiler benchmark suite exercises declaration-heavy input, repeated expressions, function calls, string literals, nested scopes, and struct dependency ordering. A dedicated C++ harness measures lexer, parser, semantic-analysis, and code-generation phases separately over nine runs and reports each median. The CLI benchmark reports median end-to-end time and range over seven runs. A separate 50-million-iteration scalar workload compares Swan, Rust, and Goose, checks output equality, and reports compile-pipeline time, runtime, and peak RSS. These measurements are workload- and runner-specific.

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
- Token kinds use canonical static `string_view` spellings; the parser moves consumed lexemes into AST strings instead of copying them from the token buffer.
- AST nodes retain source line/column only, avoiding repeated copies of token-kind and token-value strings.
- Program parsing reserves declaration storage; code generation reserves structure/function indexes and uses a hash map for struct dependency lookup.
- Semantic analysis stores built-in expression types in a compact enum. Code generation reuses the cached result instead of recursively inferring types again.
- Semantic expression types are returned as non-owning `string_view` values, avoiding a new string copy at each recursive expression-analysis step.
- Semantic name lookup uses one active-symbol hash index. Scope exit restores shadowed bindings, avoiding a reverse scan through every enclosing scope.
- Assignment analysis resolves the target once instead of recursively analyzing and looking it up a second time.
- C expression emission appends into a shared expression buffer rather than building a tree of temporary strings.
- C statement, function, struct, and typedef emission writes directly to the final output buffer.
- Code-generation function, variable, scope, and structure indexes use AST-backed `string_view` keys, avoiding duplicate name allocations while the source AST remains alive.
- Struct type membership uses transparent `string_view` lookup, avoiding a temporary string for every struct-typed field.
- Struct dependency traversal uses a generic recursive lambda instead of type-erased `std::function` dispatch.
- C string escaping and output generation append directly to reserved buffers rather than stream formatting.

These changes reduce known repeated work and avoid selected allocations. They are not a claim of a measured percentage improvement. Use the benchmark harness on the same machine before and after changes to quantify end-to-end impact.
