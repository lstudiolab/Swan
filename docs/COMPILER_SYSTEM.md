# Swan Compiler System Contract

Status: normative architecture and implementation contract.

This document defines the intended production compiler. Code, tests, command-line behavior, and future backend work must converge on this contract. Existing compatibility paths are transitional implementation details; they do not redefine the architecture.

Implementation conventions for C11 ownership, naming, formatting, diagnostics, header boundaries, and review are specified in [ENGINEERING_STYLE.md](ENGINEERING_STYLE.md). The system contract defines what the compiler must do; the engineering guide defines how its implementation should be structured and reviewed.

## 1. Non-negotiable execution model

Swan is an ahead-of-time (AOT), native-code compiler.

- Swan source is compiled before execution.
- A successful native build produces a normal object file and/or executable for the selected target ABI.
- The executable does not contain or require a Swan interpreter, JIT compiler, SVM virtual machine, or bytecode dispatch loop.
- SVM is a compiler subsystem and intermediate representation (IR), not a runtime machine.
- SIL and SVM are compiler IRs. Neither is a distributable runtime bytecode format.
- `run` means build a native executable for the host and then launch that executable as a separate operating-system process. It must never mean interpret source or IR.
- WebAssembly is a separately selected AOT output format. A WebAssembly module is not a Swan interpreter or native executable; execution requires a compatible WebAssembly host.
- No performance claim may conflate compiler throughput, executable startup, and generated-program runtime.

A future feature that introduces mandatory interpretation, JIT compilation, or VM dispatch is an architectural violation and must not be merged.

## 2. Compiler ownership

Swan owns its compiler pipeline and native backends. The production path must not depend on LLVM, LLVM IR, Clang code generation, or another compiler framework to perform Swan IR optimization, instruction selection, register allocation, machine-code generation, or object emission.

Swan-owned components include:

1. Source manager and diagnostics.
2. Lexer and parser.
3. AST and module graph.
4. Name resolution and semantic/type analysis.
5. SIL and SIL verification/optimization.
6. SVM typed SSA IR, verifier, and optimization pass manager.
7. Target descriptions, data layouts, ABIs, and relocation models.
8. Instruction selection and target legalization.
9. Machine-level IR and machine verifiers.
10. Liveness analysis, register allocation, frame layout, and spill placement.
11. Machine instruction encoding and object-file emission.
12. Native linking orchestration and executable validation.

Operating-system assemblers/linkers and system libraries may be used as explicitly documented platform tools during the transition. They are not substitutes for Swan's compiler middle-end or backend. The long-term native path must be able to emit relocatable object files without routing Swan IR through another compiler's backend.

C interoperability is a separate frontend/ABI concern. A C header parser or platform C compiler used to compile explicitly supplied C translation units must not be mistaken for Swan's backend. The supported C ABI subset, required tools, and target restrictions must be stated exactly.

## 3. Translation-unit architecture

The C compiler implementation uses a single production translation unit.

- `main.c` is the sole compiler executable entry point and coordinates the driver.
- Compiler subsystems are implemented in focused headers, grouped by responsibility.
- Headers use include guards and explicit include dependencies.
- Header implementations use `static` or `static inline` for translation-unit-local functions and data. Externally visible symbols must have one deliberate definition.
- No subsystem may include another subsystem's implementation source file.
- No duplicate implementation is permitted merely to support bootstrap and production builds.
- Header isolation tests must compile each public/implementation header in its documented inclusion context.
- Keep headers small enough to audit; split by cohesive responsibility, not arbitrary line count.

The current repository has a compatibility root `main.c` that includes `compiler/tools/swanc/main.c`. Consolidating the driver into the canonical root entry point is a migration task. Until that is completed, the included driver remains the one and only executable entry point; a second `main` must never be introduced.

Recommended subsystem boundaries:

```text
main.c
compiler/
  lib/
    Core/       source manager, arenas, diagnostics, common types
    Frontend/   lexer, parser, AST, declarations, module graph
    Sema/       name resolution, type checking, language rules
    SIL/        SIL model, builder, verifier, lowering, passes
    SVM/        typed SSA IR, builder, verifier, pass manager
    Analysis/   dominators, loops, alias/liveness and reusable analyses
    Optimize/   target-independent optimization passes
    Target/     target descriptions, data layout, ABI and features
    Machine/    machine IR, legalization, instruction selection
    RegAlloc/   liveness, register allocation, spills and frame layout
    Object/     encoders, sections, symbols, relocations and object writers
    Link/       native link driver and link-input model
    Driver/     options, project loading, pipeline and diagnostics
  include/      stable public/compatibility headers
stdlib/
tests/
benchmarks/
docs/
```

This is a responsibility map, not a mandate to move every existing file in one unsafe rename. Existing include paths remain compatible until all callers and tests have migrated.

## 4. Canonical end-to-end pipeline

The production compilation of a project follows this ordered pipeline:

```text
Project inputs
  -> source discovery and deterministic ordering
  -> source manager (one source buffer per file, stable file identity)
  -> lexing (per source file)
  -> parsing (per source file)
  -> one project/module graph
  -> import resolution and dependency ordering
  -> declaration collection across the whole project
  -> name resolution and semantic/type analysis
  -> generic/overload and ABI validation, when supported
  -> SIL generation for eligible source constructs
  -> SIL verification
  -> SIL canonicalization and optimization
  -> lowering to SVM typed SSA IR
  -> SVM verification
  -> target-independent optimization pipeline
  -> SVM verification after transformations
  -> target-specific legalization
  -> instruction selection
  -> machine IR construction and verification
  -> liveness and register allocation
  -> frame layout, spill insertion and ABI lowering
  -> machine-level optimization
  -> machine instruction encoding
  -> relocatable object emission
  -> native linking
  -> executable/object validation
```

A construct may temporarily use a documented compatibility lowering path, but it must join the same project-level semantic model and produce a diagnostic if the selected target cannot represent it. Compatibility paths must not silently claim full SIL/SVM coverage.

### 4.1 Project-wide compilation

A directory or explicit source list denotes one logical program, not a sequence of unrelated compilations.

- Discover inputs deterministically; never rely on filesystem enumeration order.
- Preserve each source file's identity, path, line/column mapping, and source range. Do not concatenate text in a way that destroys diagnostics or permits tokens to cross file boundaries.
- Parse each source file once and add its declarations to a shared project/module graph.
- Resolve declarations across all project files before checking function bodies where language rules require whole-module visibility.
- Detect duplicate declarations and import cycles deterministically.
- Perform reachability, optimization, and code generation across the complete reachable program.
- Emit one combined native object or one combined assembly artifact for the project, then link once. Multiple object files may be an internal implementation detail, but the user-facing project is one compilation unit and one final program.
- Imported modules are deduplicated by canonical module identity and loaded once per compilation.
- Parallel frontend work is permitted only for independent files and must merge results deterministically. Shared mutable compiler state must not be accessed concurrently without a defined synchronization/ownership model.

The current driver concatenates source buffers before lexing. That is a known transitional shortcut. The source manager and per-file token streams must replace it before claiming exact multi-file diagnostics or file-boundary correctness.

### 4.2 Phase contracts

Each phase has an explicit input, output, invariant, and failure behavior.

| Phase | Input | Output | Required invariant |
|---|---|---|---|
| Source manager | paths and buffers | source files | stable file IDs and source ranges |
| Lexer | one source file | tokens | valid token boundaries; diagnostics retain file/offset |
| Parser | token stream | AST | malformed syntax is rejected; no silent node substitution |
| Module graph | ASTs/imports | project graph | unique module identity; deterministic dependency graph |
| Sema | project graph | typed declarations | names, types, visibility and control flow are valid |
| SILGen | typed AST | SIL functions/modules | every emitted value has a defined type and origin |
| SIL verifier | SIL | verified SIL | valid definitions, uses, blocks, types and terminators |
| SIL optimizer | verified SIL | optimized SIL | each pass preserves verifier invariants |
| SVM lowering | typed AST/SIL | SVM module | every source operation is represented or rejected |
| SVM verifier | SVM module | verified module | valid SSA, types, CFG, PHIs, calls and terminators |
| SVM optimizer | verified SVM | optimized SVM | pass preconditions hold; verifier passes after mutation |
| Target legalizer | optimized SVM + target | legal target IR | no unsupported operation remains |
| Instruction selector | legal target IR | machine IR | each operation maps to valid target instructions |
| Register allocator | machine IR | allocated machine IR | all virtual registers assigned or spilled correctly |
| Object writer | encoded machine code | object file | sections, symbols, relocations and ABI metadata are valid |
| Linker driver | objects and libraries | executable | target ABI and all symbols/relocations resolve |

A phase must not continue after a failed prerequisite. Errors must carry a source location where one exists, plus the phase and relevant symbol/operation. Internal invariant failures are compiler errors, not user syntax errors.

## 5. SIL and SVM responsibilities

SIL is a higher-level, source-oriented optimization/lowering IR. It should preserve semantic structure long enough to perform transformations that benefit from source-level types, ownership/lifetime facts, and structured control flow.

SVM is the target-independent, typed SSA representation consumed by target legalization and machine lowering. It must not contain target-specific register numbers or encode one CPU's instruction set.

The intended boundary is:

- SILGen lowers semantically checked source into SIL.
- SIL verification validates SIL before transformations.
- SIL passes simplify and optimize SIL while preserving its semantic invariants.
- SIL lowering converts supported SIL operations into SVM operations.
- Constructs not yet expressible in SIL use a named, tested lowering fallback into SVM. The fallback is transitional and must not be hidden.
- SVM verification runs before optimization, after any pass that mutates structural invariants, and before target lowering.
- SVM optimizations are target-independent unless a pass explicitly declares a target dependency.
- Backend-specific transformations occur only after target selection.

SIL is not required to be the only lowering route on day one. It is required to have a growing, measured coverage boundary, with tests proving which constructs use it. A bridge that optimizes only a narrow subset must be described as a subset, not as complete SIL support.

## 6. Optimization and pass management

Optimization is a sequence of independently testable passes managed by an explicit pass manager.

Each pass declares:
- required analyses and IR invariants;
- analyses it preserves or invalidates;
- whether it changes control flow, memory behavior, or symbol visibility;
- whether it is target-independent;
- verifier checkpoints and diagnostic behavior.

The initial target-independent suite may include constant folding, constant propagation, dead-code elimination, CFG simplification, unreachable-block elimination, algebraic simplification, copy propagation, common-subexpression elimination, loop-invariant code motion, and scalar replacement when the relevant language and memory semantics are established.

Correctness requirements:
- No transformation may assume undefined language behavior unless the language specification explicitly defines it as undefined.
- Integer folding must respect the source type's width, signedness, overflow rules, and target-independent semantics.
- Memory transformations must respect aliasing and observable side effects.
- Calls with unknown side effects must be treated conservatively.
- PHI and CFG rewrites must preserve predecessor/value correspondence.
- Loop transformations must prove their preconditions; no heuristic may silently weaken correctness.
- Every optimization has positive and negative tests, including overflow, control-flow, side-effect, and boundary cases.

## 7. Native backend contract

Each native target backend provides a complete target description:

- architecture and subarchitecture;
- endianness and pointer width;
- scalar and aggregate data layout;
- register classes and reserved registers;
- calling convention and argument/return classification;
- stack alignment and frame rules;
- instruction encodings and legal operand forms;
- relocation types and object format;
- target feature dependencies and CPU tuning model.

Backend stages are explicit and independently verifiable:

1. Type and operation legalization.
2. Instruction selection.
3. Machine IR verification.
4. Liveness and interference analysis.
5. Register allocation, including spill/reload insertion.
6. Prologue/epilogue and frame layout.
7. ABI lowering, calls, returns, and callee-save handling.
8. Machine peephole/scheduling passes where supported.
9. Instruction encoding.
10. Object sections, symbols, relocations, and metadata.

A backend must reject an unsupported operation with a precise target-specific diagnostic. It must never emit a placeholder, substitute zero, or silently route the operation through an interpreter.

The supported source/IR subset must be recorded per backend. A backend's existence or ability to emit an object does not prove complete language support, ABI correctness, successful linking, or runtime correctness.

## 8. Object emission and linking

The long-term native path emits object files directly from Swan machine instructions. The object writer owns section layout, symbol tables, relocation records, alignment, visibility, and target object metadata for each supported format.

The linker driver may invoke the platform's native linker. It must:
- pass explicit target and ABI information;
- distinguish compiler-generated objects from user objects and libraries;
- preserve deterministic and reproducible inputs where possible;
- report missing symbols and tool failures without masking the original cause;
- avoid temporary-file collisions and clean up on every failure path;
- never claim a successful executable build when only assembly or an object was emitted.

Cross-compilation is AOT. It selects the destination target before target lowering and emits destination object code. The host must not execute foreign machine code as part of compilation.

## 9. C interoperability

C interoperability must remain an explicit ABI feature, not an accidental consequence of parsing C syntax.

- Imported C declarations have external linkage and a validated C ABI signature.
- Exported Swan functions use a documented, target-specific C ABI.
- The compiler validates argument/return types, layout, alignment, variadic rules, callbacks, and pointer semantics before claiming support.
- Unsupported aggregates, variadics, callbacks, floating-point ABI classes, or platform calling conventions are diagnosed.
- C translation units may be compiled by a platform C compiler as external project inputs, then linked with Swan-generated objects. This does not make the C compiler part of Swan's implementation.
- The current importer/toolchain subset and its Clang dependency must be documented as a tool requirement until replaced or deliberately retained as a frontend-only dependency.

## 10. Performance and resource model

Performance work must preserve correctness and be measured by phase.

Required practices:
- Use arena allocation for AST, token, SIL, SVM, and temporary analysis data where lifetimes are compilation-scoped.
- Reuse buffers and avoid repeated whole-module copies or serialization.
- Cache immutable module parse/semantic artifacts with content and compiler-version keys.
- Use hash-based symbol/module lookup with collision-safe equality checks.
- Avoid repeated linear scans in hot compiler paths when an index is justified.
- Keep pass analyses reusable and invalidate them precisely.
- Parallelize only independent work and preserve deterministic output.
- Avoid external assembler round-trips where Swan has a verified native encoder/object writer for the target.
- Keep diagnostic construction and profiling overhead out of optimized hot paths when disabled.

Measure separately:
1. Frontend throughput (files, bytes, tokens, declarations per second).
2. End-to-end compile wall time and CPU time.
3. Peak resident memory and allocation volume.
4. Optimization and backend stage timings.
5. Object and executable size.
6. Generated program runtime and startup time.

Benchmarks require warmups, repeated runs, medians and dispersion, pinned/recorded hardware and toolchain versions, correctness checksums, and identical optimization/target settings. Never claim Swan is faster than C, Rust, Swift, or another compiler from a single workload or compile-time number.

## 11. Testing and release gates

A change to a compiler phase is not complete until it has:
- unit tests for valid and invalid inputs;
- verifier tests for malformed internal IR;
- end-to-end source-to-executable tests;
- regression tests for existing supported language behavior;
- ABI tests where calls, layout, or object emission change;
- target-specific object validation and execution tests on real target hardware or a trusted emulator where available;
- multi-file tests proving one shared module and correct cross-file diagnostics;
- deterministic-build checks for stable inputs;
- benchmark results when performance is the purpose of the change.

A target is marked production-supported only when its documented source subset passes compile, link, and execution/module-validation tests. An emitted assembly listing alone is not sufficient.

## 12. Current implementation facts and migration order

The repository currently has a C11 driver, header-oriented compiler implementation, typed SSA IR, an SVM verifier/optimizer, a limited Swan-authored SIL bridge, several Swan-owned target emitters, and project-directory source collection. It also has transitional behavior that does not yet satisfy this contract:

- root `main.c` is a compatibility include; the actual driver is `compiler/tools/swanc/main.c`;
- project sources are concatenated before lexing, so per-file source identity is not fully preserved;
- SIL covers only a narrow scalar straight-line subset and starts from a complete general typed-IR candidate;
- the driver contains target-specific output/link orchestration;
- native assembly/object production still invokes external toolchain components;
- the optional LLVM target bridge routes verified SVM IR through LLVM/Clang and is not part of the desired Swan-owned native backend;
- target feature coverage and ABI completeness are unequal;
- C header import relies on Clang AST output for its currently supported declaration subset.

Migration order:

1. Establish source manager and per-file lex/parse while retaining the shared project graph.
2. Make the phase pipeline explicit and test each phase boundary.
3. Expand SIL semantics, verification, lowering coverage, and pass management.
4. Consolidate SVM pass management, analysis preservation, and verifier checkpoints.
5. Define a shared machine IR and target backend interface.
6. Complete one native backend end-to-end, including register allocation and Swan-owned object emission.
7. Migrate other backends to the same interface with per-target ABI tests.
8. Remove the LLVM code-generation bridge after all required target support has a Swan-owned implementation; do not remove it before a replacement exists if doing so would break existing documented users.
9. Consolidate object/link handling and make the root `main.c` the canonical driver.
10. Expand C ABI coverage only alongside layout, ABI, and integration tests.

This sequence deliberately avoids a destructive rewrite. Preserve existing tested behavior while replacing transitional internals behind stable interfaces. Do not mark a migration step complete merely because a header or enum has been added; require an end-to-end test that exercises the behavior.
