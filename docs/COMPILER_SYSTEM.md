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
- SVM is Swan's target-independent typed compiler IR. It is not a distributable runtime bytecode format.
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
5. SVM typed SSA IR, verifier, and optimization pass manager.
6. Target descriptions, data layouts, ABIs, and relocation models.
7. Instruction selection and target legalization.
8. Machine-level IR and machine verifiers.
9. Liveness analysis, register allocation, frame layout, and spill placement.
10. Machine instruction encoding and object-file emission.
11. Native linking orchestration and executable validation.

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
  -> direct AST-to-SVM typed SSA IR lowering
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

A construct may temporarily use a documented compatibility lowering path, but it must join the same project-level semantic model and produce a diagnostic if the selected target cannot represent it. Compatibility paths must not silently claim typed-IR or backend feature coverage.

### 4.1 Project-wide compilation

A directory or explicit source list denotes one logical program, not a sequence of unrelated compilations.

- Discover inputs deterministically; never rely on filesystem enumeration order.
- Preserve each source file's identity, path, line/column mapping, and source range. Do not concatenate text in a way that destroys diagnostics or permits tokens to cross file boundaries.
- Parse each source file once and add its declarations to a shared project/module graph.
- Resolve declarations across all project files before checking function bodies where language rules require whole-module visibility.
- Detect duplicate declarations and import cycles deterministically.
- Perform reachability, optimization, and code generation across the complete reachable program.
- Emit one assembly file and one object file per source translation unit. The project linker consumes those objects and produces one final executable. Source files remain separate throughout lexing, parsing, code generation, and object emission; project-wide declarations and semantic analysis still operate over the shared project graph.
- Imported modules are deduplicated by canonical module identity and loaded once per compilation.
- Parallel frontend work is permitted only for independent files and must merge results deterministically. Shared mutable compiler state must not be accessed concurrently without a defined synchronization/ownership model.

The driver now lexes and parses each source buffer independently and retains its source text and token storage in the project source manager. Native x86-64 project builds produce one object per Swan source file before linking. Explicit emission produces one target artifact per source across x86-64, AArch64, ARMv7, RISC-V64, WebAssembly, and LLVM IR/assembly. Cross-target `build` still emits one project-wide artifact; per-source cross-target object linking remains unfinished. Source identity is retained on declarations and diagnostics.

### 4.2 Phase contracts

Each phase has an explicit input, output, invariant, and failure behavior.

| Phase | Input | Output | Required invariant |
|---|---|---|---|
| Source manager | paths and buffers | source files | stable file IDs and source ranges |
| Lexer | one source file | tokens | valid token boundaries; diagnostics retain file/offset |
| Parser | token stream | AST | malformed syntax is rejected; no silent node substitution |
| Module graph | ASTs/imports | project graph | unique module identity; deterministic dependency graph |
| Sema | project graph | typed declarations | names, types, visibility and control flow are valid |
| SVM lowering | checked AST | SVM module | every source operation is represented or rejected |
| SVM verifier | SVM module | verified module | valid SSA, types, CFG, PHIs, calls and terminators |
| SVM optimizer | verified SVM | optimized SVM | pass preconditions hold; verifier passes after mutation |
| Target legalizer | optimized SVM + target | legal target IR | no unsupported operation remains |
| Instruction selector | legal target IR | machine IR | each operation maps to valid target instructions |
| Register allocator | machine IR | allocated machine IR | all virtual registers assigned or spilled correctly |
| Object writer | encoded machine code | object file | sections, symbols, relocations and ABI metadata are valid |
| Linker driver | objects and libraries | executable | target ABI and all symbols/relocations resolve |

A phase must not continue after a failed prerequisite. Errors must carry a source location where one exists, plus the phase and relevant symbol/operation. Internal invariant failures are compiler errors, not user syntax errors.

Warnings are non-fatal by default and must identify suspicious but not provably invalid source constructs. The driver may promote warnings to errors with `--warnings-as-errors`; when promoted, compilation stops before code generation and must not leave a newly generated output artifact. Warning checks must avoid claiming certainty where control-flow or alias information is incomplete.

## 5. SVM responsibilities

SVM is the compiler's single target-independent typed SSA representation. The production compiler lowers supported source constructs directly from the checked AST into SVM. It owns IR construction, structural and type verification, optimization pass management, and the verified representation passed to target backends.

The SVM representation must not contain target-specific register numbers or encode one CPU's instruction set. Target-dependent transformations occur only after target selection.

The canonical lowering contract is:

- The frontend produces a checked project AST and module graph.
- AST-to-SVM lowering creates typed functions, blocks, instructions, calls, and terminators.
- SVM verification validates SSA definitions/uses, types, control flow, PHI edges, call signatures, and terminators.
- Target-independent SVM passes preserve language semantics and declare their analysis requirements and invalidation behavior.
- Verification runs before optimization, after transformations that can invalidate structural invariants, and before target lowering.
- The backend legalizes and lowers verified SVM to its target-specific representation.

The x86-64 AST compatibility backend remains a deliberate, separately tested route for currently supported source features that are not represented in typed IR. It is selected by feature requirements, never as recovery after an IR/verifier/optimizer failure.

The C11 production compiler is built directly. No bootstrap compiler or secondary compiler is built.

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
- Use arena allocation for AST, token, SVM, and temporary analysis data where lifetimes are compilation-scoped.
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

The repository currently has a C11 driver, header-oriented compiler implementation, typed SSA IR, an SVM verifier/optimizer, several Swan-owned target emitters, and project-directory source collection. It also has transitional behavior that does not yet satisfy this contract:

- root `main.c` is a compatibility include; the actual driver is `compiler/tools/swanc/main.c`;
- project sources are concatenated before lexing, so per-file source identity is not fully preserved;
- the driver contains target-specific output/link orchestration;
- native assembly/object production still invokes external toolchain components;
- the optional LLVM target bridge routes verified SVM IR through LLVM/Clang and is not part of the desired Swan-owned native backend;
- target feature coverage and ABI completeness are unequal;
- C header import relies on Clang AST output for its currently supported declaration subset.

Migration order:

1. Keep source identity and per-file lex/parse stable across the shared project graph.
2. Make phase timing, allocation volume, peak RSS, and pass statistics observable in repeatable benchmarks.
3. Consolidate SVM pass management, reusable analyses, and verifier checkpoints.
4. Define a shared machine IR and target backend interface.
5. Complete one native backend end-to-end, including register allocation and Swan-owned object emission.
6. Migrate other backends to the same interface with per-target ABI tests.
7. Remove the LLVM code-generation bridge only after required target support has a Swan-owned implementation and compatibility tests pass.
8. Consolidate object/link handling and preserve the single direct C11 production build.
9. Expand C ABI coverage only alongside layout, ABI, and integration tests.

This sequence deliberately avoids a destructive rewrite. Preserve existing tested behavior while replacing transitional internals behind stable interfaces. Do not mark a migration step complete merely because a header or enum has been added; require an end-to-end test that exercises the behavior.
