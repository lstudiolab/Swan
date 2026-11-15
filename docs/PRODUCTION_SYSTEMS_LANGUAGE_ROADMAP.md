# Swan Production Systems Language Expansion

Status: implementation roadmap. This document does not claim that listed capabilities already exist.

## Goal

Swan is intended to become a general-purpose native systems language suitable for:

- AAA game engines and game servers;
- high-throughput network services and distributed systems;
- AI inference/training infrastructure and accelerator runtimes;
- operating systems, kernels, drivers, and embedded software;
- scientific computing, desktop applications, command-line tools, and libraries.

The objective is broad capability with predictable native performance, not a collection of keywords or superficial syntax compatibility. Every feature must be represented in semantic analysis, the compiler IRs, code generation, the standard library where appropriate, and tests.

Swan remains a Swan-owned ahead-of-time compiler. SIL and SVM are compiler representations, not a mandatory runtime. Do not add an LLVM dependency or silently route production code generation through another compiler framework.

## 1. Non-negotiable engineering rules

1. Preserve existing valid Swan programs and the current language syntax unless a deliberate language-version change is approved.
2. Do not invent syntax before defining its semantics and implementation path.
3. A feature is not implemented because its keyword parses. It must type-check, lower, optimize, generate code, link, execute, and have regression tests on every target that claims support.
4. Unsupported operations must produce explicit diagnostics. Never emit placeholder instructions, substitute zero, or silently fall back to interpretation.
5. Keep the source language target-independent. Target-specific details belong in target descriptions, ABI lowering, and backend legalization.
6. Maintain verifiers for SIL, SVM, and machine IR. Verify after transformations at defined checkpoints.
7. Preserve deterministic builds and per-file source locations. Do not concatenate source files before lexing.
8. Use C11 conventions and the repository's header-oriented organization. Keep implementation modular; avoid one giant compiler header.
9. Measure compiler time, generated program runtime, startup time, memory use, and binary size separately.
10. Do not claim parity with C++, Rust, Swift, or any other language until the relevant feature and ABI test suites pass.

## 2. Compiler architecture to grow toward

```text
Source files
  -> Source manager and diagnostics
  -> Lexer / parser / AST
  -> Project and module graph
  -> Name resolution / type checking / borrow and effect analysis
  -> SIL generation and verification
  -> SIL optimization and specialization
  -> SVM lowering and verification
  -> Target-independent SVM optimization
  -> Target legalization
  -> Machine IR and machine verifier
  -> Instruction selection
  -> Liveness / register allocation / spills
  -> ABI lowering / frame layout
  -> Machine optimization and encoding
  -> Swan-owned object writer
  -> Native linker driver
  -> Executable, shared library, or relocatable object
```

### Frontend and semantic model

Expand the frontend in deliberate stages:

- per-file source manager, stable source IDs, spans, and diagnostic notes;
- project/module graph with deterministic import resolution and cycle diagnostics;
- complete declaration model for functions, globals, types, methods, traits/interfaces, and visibility;
- structural and nominal types, generic parameters, constraints, and specialization;
- overload resolution and explicit conversion rules;
- closures, captures, function values, and calling conventions;
- exhaustive pattern matching and algebraic data types;
- compile-time evaluation with bounded resource use;
- precise error recovery that never converts invalid programs into apparently valid AST nodes.

### Type and memory model

The language must provide a documented model for:

- fixed-width signed and unsigned integers;
- IEEE floating-point types and explicit numeric conversions;
- booleans, characters, UTF-8 strings, byte strings, and slices;
- arrays, vectors, tuples, structs, enums, unions, and tagged unions;
- raw pointers, references, opaque handles, and function pointers;
- atomics and memory ordering;
- explicit alignment, layout, and representation attributes;
- deterministic destruction and resource cleanup;
- safe and unsafe boundaries.

For systems programming, define ownership, borrowing, aliasing, mutability, lifetimes, and destruction semantics before relying on them for optimization. Do not retrofit optimizer assumptions that the language has not specified. Provide an explicit unsafe escape hatch for low-level code, with clear contracts.

### SIL

SIL should preserve high-level semantic information long enough for:

- generic specialization and monomorphization;
- ownership and lifetime analysis;
- ARC/resource-lifetime optimization where relevant;
- closure and async lowering;
- devirtualization and inlining;
- high-level loop and collection transformations;
- diagnostics tied to source-level constructs.

SIL must have its own verifier, pass manager, pass contracts, and coverage tests. The existing SIL bridge is a limited subset; expand its verified coverage rather than describing it as complete.

### SVM

SVM is the typed, target-independent SSA layer. Grow it to represent:

- complete integer and floating-point operations;
- conversions, comparisons, selects, and control flow;
- aggregate construction, extraction, insertion, and layout operations;
- loads, stores, address calculations, and explicit memory effects;
- calls, indirect calls, intrinsics, and calling-convention metadata;
- atomics and fences;
- vector and SIMD operations;
- exception/panic and unwind edges if adopted by the language;
- coroutine/async state-machine operations before target lowering;
- debug locations and source-variable mapping.

Each opcode must define operand types, result types, side effects, trapping/overflow behavior, and verifier rules. Memory operations require alias and alignment semantics. Avoid adding an opcode until all of these are specified.

### Optimization engine

Use a pass manager with explicit analysis dependencies and invalidation. Build and test, in order:

1. constant folding and propagation;
2. dead instruction and dead block elimination;
3. CFG simplification and PHI cleanup;
4. copy propagation and canonicalization;
5. common-subexpression elimination;
6. scalar replacement and aggregate simplification;
7. alias analysis and memory forwarding;
8. loop analysis, loop-invariant code motion, induction-variable simplification;
9. inlining and interprocedural optimization;
10. vectorization and target-aware cost modeling;
11. link-time optimization across compilation units.

Every pass must preserve language semantics, have negative tests, and run under IR verification. Integer overflow, floating-point behavior, pointer aliasing, volatile accesses, atomics, and unknown calls must be handled conservatively.

## 3. Native backend and platform strategy

Use one shared machine-IR contract and independent target backends. Do not copy an entire backend to add another architecture.

Initial target families:

- x86-64: desktop and server systems;
- AArch64: Apple platforms, Android, ARM servers, and embedded systems;
- RISC-V 64: systems and embedded platforms;
- ARM 32: legacy and constrained embedded targets;
- WebAssembly: optional portable AOT output, not the native systems baseline.

For every target, implement and test:

- data layout, endianness, pointer width, and alignment;
- register classes and reserved registers;
- calling conventions and argument/return classification;
- stack alignment, frame layout, and callee-save rules;
- instruction selection and legal operand forms;
- register allocation, spills, and reloads;
- relocations, symbol visibility, object format, and TLS;
- CPU feature detection and explicit feature flags;
- unwind/debug metadata where supported;
- native object validation and executable integration tests.

A backend's existence is not evidence of complete language support. Publish a per-target feature matrix and reject unsupported types or operations precisely.

## 4. Workload requirements

### AAA games and game engines

Compiler/runtime requirements:

- predictable low-latency code generation and no mandatory garbage collector;
- deterministic allocation options, arenas, pools, custom allocators, and aligned allocation;
- SIMD for scalar/vector math, transforms, animation, and physics;
- robust multithreading, atomics, task scheduling, and lock-free building blocks;
- zero-cost abstractions where their semantics permit;
- generics, specialization, inlining, devirtualization, and LTO;
- C ABI and practical C++ interoperability for existing engines and middleware;
- shared libraries/plugins, stable exported symbols, and platform ABI support;
- debug symbols, source maps, profiling hooks, and crash/unwind support;
- GPU programming integration through explicit APIs and foreign interfaces.

The compiler is not itself a renderer or physics engine. The standard library and ecosystem need platform, math, asset, audio, input, and graphics bindings. GPU kernels require a separately defined GPU target/toolchain; CPU code generation does not imply GPU support.

### Servers and distributed systems

Requirements:

- async/await with a specified cancellation and structured-concurrency model;
- efficient futures/tasks and bounded executor primitives;
- nonblocking TCP/UDP and platform I/O completion backends;
- HTTP, TLS, DNS, WebSocket, and serialization libraries;
- atomics, synchronization, channels, and thread-safe ownership;
- predictable memory allocation, backpressure, and bounded queues;
- timers, filesystem APIs, process management, and signals;
- observability hooks for tracing, metrics, and structured diagnostics;
- shared-library ABI, plugin loading, and cross-compilation for Linux and Windows.

Runtime and standard-library work is essential here. Adding async syntax without scheduler, cancellation, I/O, and error semantics is incomplete.

### AI servers and compute

Requirements:

- efficient tensor and multidimensional-array abstractions;
- SIMD and vector types in the language and SVM;
- GPU/accelerator interoperability with explicit device memory and synchronization;
- C ABI bindings for established CPU/GPU compute libraries;
- memory pools, pinned buffers, zero-copy views, and asynchronous transfers;
- parallel execution and task graphs;
- high-throughput serialization, batching, and network serving;
- numerical correctness tests for floating-point and mixed-precision operations.

Do not claim native CUDA, ROCm, Metal, or other GPU kernel compilation until Swan has a real backend or a clearly documented interoperable external toolchain. Host-side AI serving and device-kernel compilation are separate capabilities.

### Operating systems, embedded, and low-level work

Requirements:

- freestanding compilation without libc or hosted runtime assumptions;
- no_std-style core library profile;
- explicit startup and entry-point control;
- volatile memory access and architecture intrinsics;
- atomics with target-validated memory ordering;
- interrupt-safe and allocation-free subsets;
- linker scripts, custom sections, symbol control, and relocation support;
- inline assembly with validated constraints;
- MMIO and hardware register access;
- panic handlers and configurable stack/runtime initialization.

A kernel profile must not accidentally pull in threads, filesystem, network, or hosted process facilities.

### General-purpose application development

Requirements:

- complete standard library and package/module management;
- filesystem, process, environment, path, and time APIs;
- Unicode-correct text handling and formatting;
- collections, iterators, algorithms, and range types;
- JSON and common serialization interfaces;
- testing, documentation, formatting, linting, and debugger integration;
- C interoperability and bindings to system libraries;
- stable library ABI and versioned package distribution.

## 5. Standard library structure

Keep the core library small and usable in freestanding builds. Layer hosted functionality separately.

Suggested logical modules:

- `!core`: primitive types, memory, intrinsics, panic contracts;
- `!alloc`: allocators, vectors, strings, boxes, arenas;
- `!collections`: maps, sets, queues, heaps, deques;
- `!math`: numeric algorithms, SIMD-facing APIs;
- `!io`: streams, buffered I/O, formatting;
- `!fs`: files, directories, paths;
- `!net`: sockets, DNS, HTTP building blocks;
- `!sync`: atomics, locks, channels;
- `!async`: futures, tasks, cancellation, executors;
- `!process`: process and environment APIs;
- `!time`: clocks, durations, timers;
- `!crypto`: cryptographic interfaces and vetted implementations;
- `!test`: assertions, fixtures, test runner.

These are module boundaries, not a request to add a large set of aliases for the same operation. Keep one canonical printing/formatting API and build convenience on top of it.

## 6. C ABI and ecosystem interoperability

C ABI support is a foundational interoperability layer, not a substitute for Swan-native semantics.

- Define target-specific ABI rules for scalars, aggregates, variadics, callbacks, and alignment.
- Support importing declarations and exporting Swan functions with explicit ABI annotations.
- Validate layouts against the platform ABI.
- Link external object files and system libraries.
- Generate stable headers for exported APIs.
- Document ownership and error conventions at FFI boundaries.
- Add C integration tests for each supported target.

C++ interoperability can follow with explicit support for a bounded, documented subset. Do not promise arbitrary C++ templates or exceptions through a C-only ABI.

## 7. Implementation phases and exit gates

### Phase A — compiler foundation

- Replace source concatenation with a per-file source manager and shared module graph.
- Expand semantic analysis and typed AST coverage.
- Make SIL/SVM coverage measurable.
- Define machine IR, target descriptions, and pass-manager interfaces.
- Add end-to-end tests for every currently supported source construct.

Exit gate: deterministic multi-file builds, precise diagnostics, verified IR, and no silent AST/IR fallback.

### Phase B — language and memory model

- Complete scalar types, aggregates, pointers, references, function values, and generics.
- Specify ownership, borrowing, destruction, unsafe code, and FFI rules.
- Add layout and ABI tests.
- Expand SIL and SVM to cover the specified semantics.

Exit gate: each feature compiles and runs through the verified IR path on the primary native target.

### Phase C — native backend completion

- Build one production backend end-to-end before expanding backend count.
- Add machine IR, register allocation, frame lowering, object writing, relocations, and ABI conformance.
- Remove reliance on an external assembler for the completed backend where feasible.
- Port the shared backend contract to additional architectures.

Exit gate: native object validation, link tests, ABI tests, execution tests, and documented target coverage.

### Phase D — concurrency and hosted runtime

- Add threads, atomics, synchronization, async, cancellation, and executors.
- Implement filesystem, networking, process, and time libraries.
- Add stress, race, cancellation, and throughput tests.

Exit gate: production-grade examples for a network server and a concurrent service, with resource and failure behavior documented.

### Phase E — game and compute support

- Add SIMD, vectorization, custom allocators, profiling, and shared-library support.
- Add GPU interoperability and then evaluate native GPU code generation as a distinct backend project.
- Add math, tensor, and task-graph libraries.

Exit gate: validated game-engine integration and AI inference-serving examples; no unsupported GPU claims.

### Phase F — ecosystem and portability

- Package manager, dependency resolution, reproducible builds, documentation, formatter, linter, debugger support.
- Cross-compilation and CI execution matrix.
- Stable library and FFI versioning.

Exit gate: users can build, test, package, and distribute real applications without manually assembling compiler internals.

## 8. Benchmark and compatibility policy

Maintain separate benchmark suites for:

- compile throughput and peak compiler memory;
- integer, floating-point, SIMD, and memory-bound runtime;
- allocation and multithreaded scaling;
- network throughput, tail latency, and connection concurrency;
- game-oriented math/physics kernels and frame-time variance;
- AI preprocessing, tensor kernels, transfer overhead, and inference serving;
- executable startup and binary size.

Compare equivalent algorithms, input data, optimization levels, target CPUs, and compiler versions. Validate output checksums. Use warmups, repeated runs, medians, and dispersion. Never use one synthetic benchmark to claim overall language superiority.

## 9. Definition of done

A capability is marked:

- **Planned**: design exists, no implementation claim.
- **Frontend**: syntax and semantic checks work.
- **IR**: verified SIL/SVM representation and optimizer tests exist.
- **Backend**: native lowering, ABI, object, and link tests pass.
- **Runtime**: standard-library/runtime behavior and stress tests pass.
- **Production**: documented target support, integration examples, regression suite, and performance measurements pass.

The compiler's public capability table must use these states. Never label a feature production-ready based only on parsing, a demo, or one target's assembly output.
