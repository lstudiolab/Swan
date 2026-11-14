# Swan Compiler Engineering Style

This guide adapts the engineering qualities associated with Apple's compiler and systems codebases to Swan's C11 implementation. It is a design standard, not a claim that Swan is written by or endorsed by Apple. Do not copy proprietary source code or imitate superficial syntax at the expense of C correctness.

The goal is code that is predictable, locally understandable, mechanically verifiable, and maintainable as the compiler grows.

## 1. Design principles

1. Make invalid states difficult to represent. Validate inputs at subsystem boundaries and use explicit enums, tagged records, and checked constructors.
2. Prefer explicit ownership. Every allocation has one owner, a documented lifetime, and one destruction path. Borrowed pointers are const whenever mutation is not required.
3. Keep responsibilities narrow. A file and a function should have one primary reason to change.
4. Make data flow visible. Prefer explicit parameters and return values over hidden global state.
5. Fail closed. Unsupported syntax, IR, target features, ABI forms, or relocations produce diagnostics; they never become placeholders or interpreter fallbacks.
6. Verify boundaries. Validate parsed AST, SIL, SVM, machine IR, and object metadata at phase boundaries.
7. Optimize measured hot paths. Use algorithmic improvements before micro-optimizations and keep benchmark evidence with performance changes.
8. Preserve deterministic behavior. Stable source ordering, symbol resolution, diagnostics, pass ordering, and artifact naming are part of compiler quality.

## 2. C11 source conventions

- Use four spaces for indentation; do not use tabs.
- Put one declaration or statement on each line unless a short initializer is clearer as a compact group.
- Use braces for all control-flow bodies, including single-statement branches.
- Keep lines short enough to review comfortably; split complex expressions into named intermediate values.
- Use descriptive snake_case names for functions, variables, and fields. Prefix compiler-private APIs consistently with swan_.
- Use PascalCase for public struct and enum type names, and SWAN_ for macros and enum constants.
- Prefer const for read-only inputs and borrowed data.
- Use fixed-width integer types for serialized, target, and IR identifiers. Use size_t for host memory sizes and collection indices.
- Check arithmetic before allocation-size calculations and before narrowing conversions.
- Do not rely on signed overflow, implicit truncation, host endianness, locale, or unspecified evaluation order.
- Avoid clever macros. A small static inline function is usually easier to type-check and debug.
- Avoid hidden global mutable state. Where unavoidable, isolate it and make its scope explicit.
- Keep public headers self-contained, include only direct dependencies, and use include guards.
- Header-defined implementation functions must be static or static inline unless one deliberately owned external definition is required.
- Do not use compiler-specific extensions in the portable C11 core without a guarded fallback and a documented reason.

## 3. Function design

A function should expose its contract in its signature and control flow.

- Validate pointer arguments and enum/range inputs before dereferencing or indexing.
- Return a success value separately from output data; do not overload valid values as error sentinels when ambiguity is possible.
- Initialize output parameters to a safe empty state before work begins.
- Use one cleanup path when a function owns multiple resources.
- Do not silently swallow allocation, I/O, child-process, or verification failures.
- Keep diagnostic construction at the point where the failure is detected.
- Extract repeated logic into a named helper only when the helper has a coherent invariant.
- Avoid functions that mix parsing, semantic analysis, optimization, code generation, file output, and process execution.

## 4. Ownership and memory

Swan uses explicit ownership with arena allocation for compilation-scoped graphs.

- Source buffers and source identities belong to the source manager.
- Tokens borrow source identity and own or reference token text according to the token-stream lifetime.
- AST nodes belong to the program arena; AST nodes do not individually free arena memory.
- SIL/SVM modules own their dynamic instruction, block, and metadata arrays.
- Backend temporary state belongs to the backend invocation and is destroyed before returning.
- Object buffers belong to the object writer until ownership is transferred to the caller.
- Never retain pointers into temporary token streams, stack buffers, or reallocatable arrays.
- Document whether a function borrows, consumes, or returns ownership.
- Every destructor accepts an empty or partially initialized value and is safe to call once on that value.
- On failure, release every resource acquired by the current operation; do not leave half-mutated IR in a state that later phases may consume.

## 5. Compiler architecture

The source tree follows subsystem boundaries rather than historical implementation accidents:

- Basic: source identity, diagnostics, arenas, checked allocation, profiling.
- Parse: lexer, token stream, parser and syntax recovery.
- AST: source-oriented syntax representation.
- Sema: declaration indexing, name resolution, type checking and language rules.
- SIL: typed source-oriented IR and high-level transformations.
- IR: typed SSA SVM, builders, verifier, analyses and pass manager.
- Target: target triples, data layout, ABI and feature descriptions.
- Machine: target legalization, instruction selection and machine IR.
- RegAlloc: liveness, register allocation, spills and frame layout.
- Object: machine encoders, sections, symbols and relocations.
- Driver: command-line policy, project loading and orchestration.

The executable entry point coordinates the driver. Subsystems do not invoke one another through command-line processes or hidden global state except at explicitly documented platform-tool boundaries.

## 6. IR and optimization quality

- Every IR opcode has documented operand count, operand types, result type, side effects, memory behavior, and legal terminator context.
- Every builder validates ownership, ranges, and operand storage before mutation.
- Every verifier checks storage invariants before indexing and checks semantic invariants before backend consumption.
- Use indexed definition/use information and cached analyses when repeated scans become a measured hot path.
- Dominance, post-dominance, loop structure, alias facts, and liveness should be reusable analyses with explicit invalidation.
- Each optimization declares required analyses and which analyses it preserves or invalidates.
- Mutating passes must either preserve SSA/CFG invariants or trigger the corresponding rebuild and verifier checkpoint.
- A pass must not assume an operation is pure unless its opcode contract proves it.
- Constant folding must use explicit source-width arithmetic and avoid host-language undefined behavior.
- Transformations require focused tests for both successful rewrites and cases that must not be rewritten.

## 7. Diagnostics

Diagnostics are part of the compiler's public interface.

- Preserve source file identity and source ranges from lexing through AST and semantic analysis.
- Report file path, line, column, phase, and a concise explanation whenever available.
- Do not merge source text in a way that changes token boundaries or destroys source provenance.
- Distinguish user errors, unsupported language features, target limitations, I/O failures, and internal compiler invariant failures.
- Keep internal implementation details out of ordinary user-facing diagnostics; provide detailed context in debug builds or explicit verbose modes.

## 8. Tests and review

Every compiler change should include the smallest test that proves its contract.

- Add unit tests for the new behavior and negative tests for invalid inputs.
- Add IR verifier tests for malformed internal states, not only source programs.
- Add multi-file tests for cross-file references, duplicate declarations, imports, deterministic ordering, and diagnostics.
- Run the complete test suite after touching shared compiler infrastructure.
- Run sanitizers and static analysis for memory-management and bounds-sensitive changes where supported.
- Benchmark before and after changes using the same compiler flags, target, inputs, hardware, and warmup policy.
- Review generated assembly/object metadata when backend or ABI behavior changes.
- Do not merge a change that merely adds an enum, file, or option without an end-to-end behavior test.

## 9. Preferred implementation sequence

For architectural work, prefer incremental replacements behind stable contracts:

1. Source manager with per-file token streams and stable source IDs.
2. Declaration index and deterministic project/module graph.
3. Reusable analysis manager and cached dominance/loop analyses.
4. Explicit pass manager with preservation/invalidation declarations.
5. Shared machine IR and target backend registry.
6. Swan-owned object writer for one fully tested native target.
7. Additional backends migrated to the same contracts.
8. Canonical root driver and removal of compatibility-only entry points.

A subsystem is complete only when its implementation, tests, diagnostics, and documentation agree.
