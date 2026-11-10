# Swan

Swan is a compiled programming language with a Swan-owned frontend, typed intermediate representation, optimizer, and machine-code backends. The compiler does not transpile Swan source to C. Its host compiler is C11; a deliberately small, Swan-authored SIL optimizer is compiled into the final compiler during bootstrap.

Licensed under Apache-2.0.

## Current compiler

The compiler has two cooperating source-lowering paths:

- **Typed IR path:** preferred for the supported scalar/control-flow subset. Swan source is lowered through the Swan-authored SIL bridge when eligible, otherwise through the general AST-to-IR generator. The resulting typed SSA IR is verified, optimized, and sent to a selected backend.
- **x86-64 AST compatibility path:** retains currently supported frontend features not yet represented in typed IR, notably arrays, output/logging, and runtime-lazy standard-library imports.

The compatibility path is intentional. A construct supported by the x86-64 frontend is not automatically supported by every IR backend.

### Pipeline

```text
.swan
  -> UTF-8 validation and lexer
  -> indentation-aware parser / AST
  -> C-header import + standard-library module loading
  -> semantic analysis and reachability
  -> source lowering:
       Swan SIL bridge (eligible scalar functions)
       general AST -> typed SSA IR (fallback)
       AST -> x86-64 compatibility backend (where required)
  -> typed IR verification
  -> target-independent optimization
  -> selected Swan backend:
       x86-64 | AArch64 | ARMv7 | RISC-V64 | WASM32 | WASM64 | SVM
       or explicit LLVM target-triple bridge
  -> assembly / object / WebAssembly / .swvm
  -> assembler and linker where applicable
```

### Implemented typed-IR optimization

The optimizer performs width-aware constant folding, constant-select folding, CFG and constant-branch simplification, unreachable-block removal, algebraic simplification, trivial-PHI copy propagation, dominance-aware common-subexpression elimination, conservative loop-invariant code motion, post-hoisting CSE, and dead-code elimination. DCE propagates liveness through a value worklist. IR verification runs before optimization, after individual transformations, and after final cleanup. These passes apply to the typed-IR path; the AST compatibility backend has separate optimizations.

### SVM compiler core

SVM is the target-independent compilation engine, not a CPU target and not an interpreter. It owns the shared path from typed source lowering through IR verification and optimization. The same verified, optimized IR is passed to the selected machine backend:

- x86-64
- AArch64 / ARM64
- ARMv7 / ARM32
- RISC-V
- WebAssembly 32/64
- additional LLVM-supported target triples through the explicit LLVM bridge

Each backend performs its own instruction selection, target legalization, ABI lowering, register allocation or local assignment, and assembly/module emission. No target is allowed to execute the IR through an interpreter loop.

The migration is in progress. The CLI still exposes the legacy `--target svm`, `swan vm`, and `.swvm` IR-interpreter path for compatibility; these are not the destination architecture and must be removed after compiled-target coverage and tests replace them. See [SVM Compiler Architecture](docs/SVM_ARCHITECTURE.md).

### Fail-fast behavior

The semantic checker, IR verifier, and code generators validate the constructs they receive. Unknown or unsupported nodes are diagnosed instead of silently becoming empty statements or zero-valued code. This does not mean every parser keyword is implemented: the editor vocabulary and token registry are broader than the currently supported semantic and backend subset.

## Build and run

Requirements: a C11 compiler, POSIX shell, and a host assembler/linker available through `cc`.

```sh
sh build.sh
./build/swan run examples/hello.swan
```

Build an executable:

```sh
./build/swan build examples/hello.swan -o hello
./hello
```

Emit x86-64 assembly:

```sh
./build/swan examples/hello.swan --emit-asm -o hello.s
```

Inspect supported target names:

```sh
./build/swan --list-targets
```

The build uses a temporary C11 bootstrap executable to compile the Swan-authored SIL stage sources into assembly, assembles that output, and links the resulting object into the final compiler. The final executable is `build/swan`.

## Swan syntax

Swan source uses indentation-based blocks introduced by a colon. Braces are not block delimiters. `func` is the canonical function keyword; `fn` remains a compatibility alias. Semicolons are optional statement separators.

```swan
func main() -> int:
    let message = "Hello, World!"
    print(message)
    return 0
```

The lexer validates UTF-8, accepts an optional UTF-8 BOM, preserves UTF-8 string bytes, and accepts non-ASCII identifier code points. Columns are byte-based. Full Unicode XID classification and normalization are not implemented.

Dot notation forms a single access path and can qualify a callable name:

```swan
Foundation.abs_int(-42)
self.var.player
```

The parser can represent dotted paths; full struct-instance field and method resolution is not yet complete. Do not interpret the existence of a keyword or parsed AST node as proof of end-to-end implementation.

## Language and backend boundaries

The currently implemented x86-64 frontend/backend combination includes signed integer and boolean scalar expressions, local bindings, functions and calls, conditional and loop control flow, short-circuit boolean operators, and the established array/output compatibility features. The exact subset differs between the AST and typed-IR paths.

The typed-IR generator currently handles scalar integer/boolean expressions, direct calls, mutable and immutable locals, assignment, if/else, while, break/continue, and short-circuit AND/OR. The IR data model and verifier represent additional operations, including integer widths, pointer values, arithmetic, comparisons, select, PHI, calls, and local memory. An IR operation existing in the data model does not guarantee that every source construct or target backend can generate it.

| Target | Current status |
|---|---|
| x86-64 System V Linux | Native executable and assembly; typed-IR scalar path plus AST compatibility path |
| AArch64 Linux | Swan-owned ELF object/assembly for a limited scalar IR subset |
| ARMv7 Linux, ARM state | Swan-owned ELF object/assembly for a limited i32/i1 subset |
| RISC-V64 Linux | Swan-owned ELF object/assembly for a limited scalar IR subset |
| WebAssembly 32 | Swan-owned WASM module for the implemented scalar/control-flow subset |
| WebAssembly 64 | Swan-owned memory64 module emission for its implemented subset |
| SVM | Portable verified-IR interpreter and versioned `.swvm` artifact |
| Other LLVM triples | Optional explicit `--target-triple` bridge; depends on installed Clang/LLVM target support and the Swan typed-IR subset |

Object emission is not proof of execution or complete ABI integration on the destination platform. Cross-target runtime linking, full standard-library support, aggregate ABI lowering, and complete feature parity are not implemented.

### Cross-target examples

```sh
# Swan-owned AArch64 ELF object
./build/swan build app.swan --target aarch64 -o app-aarch64.o

# Swan-owned WebAssembly module
./build/swan build app.swan --target wasm32 -o app.wasm

# Portable SVM module
./build/swan build app.swan --target svm -o app.swvm
./build/swan vm app.swvm

# Explicit LLVM portability path
./build/swan build app.swan --target-triple aarch64-unknown-linux-gnu -o app.o
./build/swan build app.swan --target-triple x86_64-pc-windows-msvc -o app.obj
./build/swan build app.swan --target-triple arm64-apple-macosx14.0 --emit-llvm -o app.ll
```

The LLVM bridge is opt-in. It lowers verified Swan typed IR to LLVM IR, then uses the installed Clang/LLVM toolchain for assembly or object emission. It is not the default backend and does not make unsupported Swan language features portable. Use `--cpu` and repeatable `--target-feature` options only with `--target-triple`.

## Arrays and memory

The x86-64 compatibility backend supports zero-initialized `u32` arrays:

- `array(length)` and `array_u32(length)` provide bounds-checked access.
- `array_unchecked(length)` and `array_u32_unchecked(length)` omit per-access bounds checks; callers must prove index validity.
- Checked arrays store their length in an eight-byte header before the element data.

Small fixed-size arrays that do not escape can use a native stack-frame fast path. Other allocations follow the backend's allocation path. This optimization is a backend implementation detail; it does not change the array indexing contract.

### SAMC and explicit manual ownership

SAMC (Swan Automatic Memory Cleanup) tracks eligible local array allocations at compile time and emits cleanup without runtime reference counting.

```swan
func main() -> int:
    let values = array(128)
    values[0] = 42
    return 0
```

Use `manual var` when an eligible allocation must be released explicitly:

```swan
func main() -> int:
    manual var values = array(128)
    values[0] = 42
    free(values)
    return 0
```

The strict manual form is limited to direct array allocations with a single local owner. `free(values)` is parsed as a dedicated release statement, not as an ordinary user-function call. The checker rejects leaks at scope exit, double-free, use-after-free, aliasing/copying, passing or returning the owner, and incompatible ownership states across branches or loops. Ownership transfer, borrowing, destructors, and manually owned general heap objects are not implemented. Legacy `store` syntax remains accepted for compatibility and does not enable the new strict ownership checks.

## Imports and Foundation

The standard-library loader resolves and checks imported Swan modules. The Foundation source module is located at `stdlib/foundation.swan`. Runtime-lazy imported functions are built into per-module shared libraries for functions reachable from `main`; cached resolver stubs load a module on its first executed call. Keep the generated `.modules/` directory beside the executable.

```swan
import foundation

func main() -> int:
    echo!("Hello from Swan")
    echo_int(abs_int(-42))
    return 0
```

`echo!("literal")` is a compiler-recognized output intrinsic for a string literal. Foundation includes `echo_int` and `echo_bool`, plus integer helpers `abs_int`, `min_int`, `max_int`, `clamp_int`, `sign_int`, `square_int`, `cube_int`, `power_int`, `factorial_int`, `sum_to_int`, `average_int`, `is_even`, `is_odd`, and `in_range_int`.

Set `SWAN_STDLIB` or pass `--stdlib <directory>` to select a standard-library directory.

## C header interoperability

The native C header importer uses Clang's C AST dump to read supported declarations. Imported functions become external declarations in Swan's AST and typed IR; the native x86-64 System V backend emits real C ABI calls.

```swan
import C "math_api.h"

func main() -> int:
    return c_add(20, 22)
```

```sh
cc -std=c11 -O2 -c library.c -o library.o
./build/swan build app.swan --link library.o -o app
```

The initial importer supports up to 16 scalar integer/bool parameters and scalar integer/bool or void returns. Pointers, structs, unions, callbacks, variadic functions, floating-point ABI classification, C++ mangling, and non-System-V platform calling conventions are outside the supported importer/ABI subset. Clang must be installed for sources using `import C`. Linker inputs are currently accepted only by the native x86-64 executable path.

## Swan-authored SIL

The bootstrap SIL sources live in `compiler/sil/`. The current SIL is a small scalar SSA builder/optimizer, not the full Swan language IR and not a replacement for the general IR generator. It supports parameter, constant, add/subtract/multiply, equality, signed-less-than, copy, and return records in a five-u32-word instruction format. Its current builder is bounded to 256 value IDs and 256 instructions, with packed non-negative immediates up to 16 bits.

The bridge first constructs a complete typed-IR candidate, then replaces eligible straight-line, single-return scalar functions with SIL-lowered functions. Functions outside that subset keep the general typed-IR lowering. Both routes enter the same typed-IR verification, optimization, and backend stages. SIL control flow, general calls, variables, memory operations, loops, and imports are not yet lowered by the SIL builder.

## Profiling and benchmarks

Compiler-stage profiling:

```sh
./build/swan --profile examples/hello.swan
# or
./build/swan run examples/hello.swan --profile
```

The report measures source reading, lexing, parsing, imports, semantic analysis, reachability, IR generation, IR optimization, code generation, assembly output, linking, and execution where applicable.

Repeatable compiler pipeline benchmark:

```sh
python3 benchmarks/compiler_pipeline.py
```

The benchmark warms up and reports median timings across compiler stages for its included workloads. It is a measurement tool, not evidence of a particular performance ranking.

The repository also includes runtime comparison workloads, including `benchmarks/binary_search.py`. Run benchmarks on controlled hardware and compare checksums before comparing speed. Do not infer that Swan is faster than another compiler from compilation time alone.

## Tests

Run the complete repository test script:

```sh
sh test.sh
```

The script builds the compiler, checks header isolation and C11 compilation, runs C ABI and UTF-8 tests, profiling tests, smoke/regression tests, Foundation and lazy-import tests, architecture/backend tests, and the keyword registry test. Cross-target object validation does not establish native execution or full ABI correctness on those targets.

## Repository map

- `main.c` — CLI orchestration and compiler entry point.
- `compiler/include/swan/` — lexer, parser, AST, semantic analysis, IR, optimizer, VM, code generators, emitter, linker, and support headers.
- `compiler/sil/` — Swan-authored SIL builder and passes used during bootstrap.
- `stdlib/` — source-level standard library.
- `tests/` — compiler, runtime, ABI, regression, and target tests.
- `benchmarks/` — compiler and runtime measurement programs.
- `docs/` — architecture, pipeline, performance, audit, and testing notes.

## Documentation policy

The parser keyword registry is not a feature checklist. A feature is described as implemented only when its parsing, semantic rules, lowering, backend support, and tests establish that status. Target claims always state their supported subset. Performance claims require reproducible benchmark data.
