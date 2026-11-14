# Swan

Swan is a compiled programming language. Its production compiler, `swanc`, is implemented in C11 and owns the lexer, parser, semantic analysis, typed IR, optimizer, SVM pipeline, and native/cross-target backends. Swan source is compiled ahead of execution; it is not interpreted.

Licensed under Apache-2.0.

## Compiler system contract

The production architecture is specified in [docs/COMPILER_SYSTEM.md](docs/COMPILER_SYSTEM.md). The C11 implementation follows the code ownership, subsystem, diagnostics, and review standards in [docs/ENGINEERING_STYLE.md](docs/ENGINEERING_STYLE.md). It defines Swan as an AOT compiler with a Swan-owned SIL/SVM pipeline, native machine-code backends, project-wide compilation, strict phase invariants, and no interpreter, JIT, or bytecode runtime. The document also distinguishes implemented behavior from migration requirements; it is the source of truth for compiler architecture changes.

## Language guide

The programmer-facing syntax, coding conventions, complete reserved-keyword list, operator precedence, standard library, and current feature boundaries are documented in [docs/LANGUAGE_GUIDE.md](docs/LANGUAGE_GUIDE.md).

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
       x86-64 | AArch64 | ARMv7 | RISC-V64 | WASM32 | WASM64
       or explicit LLVM target-triple bridge
  -> assembly / object / WebAssembly module
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

The `swan_svm_codegen` dispatcher selects the target backend after common lowering and optimization. Each backend performs its own instruction selection, target legalization, ABI lowering, register allocation or local assignment, and assembly/module emission. No target executes the IR through an interpreter loop.

The legacy `--target svm`, `swan vm`, `.swvm` serializer, and IR interpreter have been removed from the production CLI and target registry. SVM is the compiler engine shared by all compiled targets. See [SVM Compiler Architecture](docs/SVM_ARCHITECTURE.md).

### Fail-fast behavior

The semantic checker, IR verifier, and code generators validate the constructs they receive. Unknown or unsupported nodes are diagnosed instead of silently becoming empty statements or zero-valued code. This does not mean every parser keyword is implemented: the editor vocabulary and token registry are broader than the currently supported semantic and backend subset.

Compilation is phase-gated: a failed read, lex, parse, import, semantic, IR verification, optimization, code-generation, assembly, or link step returns a non-zero status and prevents later phases from running. The compiler also emits non-fatal semantic warnings for suspicious constructs such as self-assignment, literal division by zero, constant conditions, potentially non-terminating constant-true loops, and source functions unreachable from `main`. Use `--warnings-as-errors` to stop before code generation whenever any warning is reported. Warnings are conservative bug indicators, not proof that the program is incorrect.

## Build and run

Requirements: a C11 compiler, POSIX shell, and a host assembler/linker available through `cc`.

```sh
sh build.sh
./build/swancc run examples/hello.swan
```

Build an executable:

```sh
./build/swancc build examples/hello.swan -o hello
./hello
```

Emit x86-64 assembly:

```sh
./build/swancc examples/hello.swan --emit-asm -o hello.s
```

Inspect supported target names:

```sh
./build/swancc --list-targets
```

The build uses a temporary C11 bootstrap executable to compile the Swan-authored SIL stage sources into assembly, assembles that output, and links the resulting object into the final compiler. The production executable is `build/swanc`; `build/swan` remains as a compatibility alias.

## Swan syntax

Swan source uses indentation-based blocks introduced by a colon. Braces are not block delimiters. `func` is the canonical function keyword; `fn` remains a contextual compatibility spelling, not a reserved word. Semicolons are optional statement separators.

The exact reserved-keyword set is:

```text
func struct let var null if else const type match guard block while loop
true false break continue return as from self pass upto init deinit enum
throw throws rethrow catch
```

This is a 31-word, case-sensitive set. `import` and `export` are contextual file-scope spellings; they are not reserved words. Other removed spellings (including `free`, `manual`, `defer`, `bool`, and `int`) are identifiers.

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
| SVM | Shared target-independent compilation engine; not a runtime target |
| Other LLVM triples | Optional explicit `--target-triple` bridge; depends on installed Clang/LLVM target support and the Swan typed-IR subset |

Object emission is not proof of execution or complete ABI integration on the destination platform. Cross-target runtime linking, full standard-library support, aggregate ABI lowering, and complete feature parity are not implemented.

### Cross-target examples

```sh
# Swan-owned AArch64 ELF object
./build/swanc build app.swan --target aarch64 -o app-aarch64.o

# Swan-owned WebAssembly module
./build/swanc build app.swan --target wasm32 -o app.wasm

# Explicit LLVM portability path
./build/swanc build app.swan --target-triple aarch64-unknown-linux-gnu -o app.o
./build/swanc build app.swan --target-triple x86_64-pc-windows-msvc -o app.obj
./build/swanc build app.swan --target-triple arm64-apple-macosx14.0 --emit-llvm -o app.ll
```

The LLVM bridge is opt-in. It lowers verified Swan typed IR to LLVM IR, then uses the installed Clang/LLVM toolchain for assembly or object emission. It is not the default backend and does not make unsupported Swan language features portable. Use `--cpu` and repeatable `--target-feature` options only with `--target-triple`.

## Arrays and memory

The x86-64 compatibility backend supports zero-initialized `u32` arrays:

- `array(length)` and `array_u32(length)` provide bounds-checked access.
- `array_unchecked(length)` and `array_u32_unchecked(length)` omit per-access bounds checks; callers must prove index validity.
- Checked arrays store their length in an eight-byte header before the element data.

Small fixed-size arrays that do not escape can use a native stack-frame fast path. Other allocations follow the backend's allocation path. This optimization is a backend implementation detail; it does not change the array indexing contract.

### Automatic lifetime cleanup

Swan has no source-level `free` keyword or manual-allocation modifier. The compiler emits cleanup for eligible local arrays when their last use has passed, with guarded cleanup on function exit and early returns. Small fixed-size, non-escaping arrays can use native stack-frame storage.

This is currently a limited x86-64 AST-backend lifetime optimization, not a complete ownership runtime. General ownership transfer, borrowing, aggregate and heap-object lifetimes, `deinit` execution, and exception unwinding are not implemented across the compiler. The `null`, `throw`, `throws`, `rethrow`, and `catch` words are reserved, but their complete runtime semantics are not yet implemented. In particular, do not rely on exception paths to release arbitrary owned values until exception lowering and cleanup are implemented and tested.

## Imports and Foundation

The standard-library loader resolves and checks imported Swan modules. The Foundation source module is located at `stdlib/foundation.swan`. Imported Swan modules are bundled into the program's single assembly output by default, with unreachable imported functions omitted. Use `--lazy-imports` to opt into the alternative per-module shared-library model; its cached resolver stubs load a module on its first executed call, and the generated `.modules/` directory must remain beside the executable.

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
./build/swanc build project/ -o app
```

Swan can also export a C-callable function. The `export C` marker keeps the source-level function name as its native C ABI symbol:

```swan
export C func swan_add(left: i64, right: i64) -> i64:
    return left + right
```

A C translation unit in the same project directory can declare and call `swan_add`; its generated assembly is bundled with the Swan assembly, so both languages are linked as one native translation unit. Exported and imported functions currently use the x86-64 System V scalar/pointer calling convention.

The importer supports up to 16 scalar integer/bool parameters and scalar integer/bool or void returns. Opaque pointer parameters and returns are passed as machine-word `i64` values on x86-64 System V; Swan cannot yet dereference those imported pointers or describe pointee layouts. Structs/unions passed by value, callback invocation, variadic functions, floating-point ABI classification, C++ mangling, and non-System-V platform calling conventions remain unsupported. Clang must be installed for sources using `import C`. Linker inputs are currently accepted only by the native x86-64 executable path.

## Swan-authored SIL

The bootstrap SIL sources live in `compiler/lib/SIL/Runtime/`, with the C-to-SIL bridge in `compiler/lib/SIL/SILBridge.h`. The current SIL is a small scalar SSA builder/optimizer, not the full Swan language IR and not a replacement for the general IR generator. It supports parameter, constant, add/subtract/multiply, equality, signed-less-than, copy, and return records in a five-u32-word instruction format. Its current builder is bounded to 256 value IDs and 256 instructions, with packed non-negative immediates up to 16 bits.

The bridge first constructs a complete typed-IR candidate, then replaces eligible straight-line, single-return scalar functions with SIL-lowered functions. Functions outside that subset keep the general typed-IR lowering. Both routes enter the same typed-IR verification, optimization, and backend stages. This lets a project mix SIL-lowered Swan functions with ordinary IR-lowered code and imported C declarations; C calls remain represented in typed IR and preserve their external ABI symbols. SIL itself does not yet encode general calls, control flow, local variables, memory operations, or loops. The bridge is therefore a staged lowering path, not yet a complete replacement for AST-to-IR lowering.

## Profiling and benchmarks

Compiler-stage profiling:

```sh
./build/swanc --profile examples/hello.swan
# or
./build/swanc run examples/hello.swan --profile
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
- `compiler/include/swan/` — stable compatibility/public headers.
- `compiler/lib/` — compiler implementation, organized by subsystem (AST, Basic, CodeGen, Driver, IR, Parse, SIL, Sema, Serialization).
- `compiler/lib/SIL/Runtime/` — Swan-authored SIL builder, verifier, and optimization passes.
- `stdlib/` — source-level standard library.
- `tests/` — compiler, runtime, ABI, regression, and target tests.
- `benchmarks/` — compiler and runtime measurement programs.
- `docs/` — architecture, pipeline, performance, audit, and testing notes.

## Documentation policy

The parser keyword registry is not a feature checklist. A feature is described as implemented only when its parsing, semantic rules, lowering, backend support, and tests establish that status. Target claims always state their supported subset. Performance claims require reproducible benchmark data.

## Whole-project compilation

Pass a source directory to compile every top-level `.swan` file in that directory as one program. Swan declarations are parsed into one shared program, then analyzed, optimized, and code-generated together. Top-level `.c` files in the same directory are compiled to native assembly and appended to the same output assembly file.

```text
project/
  main.swan
  math.swan
  physics.swan
  native.c
  native.h
```

```sh
# One directory -> one program -> one combined assembly file
./build/swancc build project --emit-asm -o build/project.asm

# Or compile and link that combined assembly into one executable
./build/swancc build project -o build/project
```

The same works with explicit source lists:

```sh
./build/swancc build src/main.swan src/math.swan src/physics.swan src/native.c -o app
```

Directory discovery is currently non-recursive: place project source files in the selected directory, or pass nested source files explicitly. Imported Swan modules are bundled by default; `--lazy-imports` intentionally opts out of the single-file model. The single-assembly guarantee applies to Swan and C source inputs for the native x86-64 target. Prebuilt object files and external system libraries remain normal linker inputs. Cross-target support for C source bundling and complete cross-platform C ABI interoperability are still being developed.

The C header importer currently supports the documented scalar C ABI subset. Pointer and aggregate layouts, callbacks, variadic functions, and broader platform ABI support require additional implementation; they are not implied by accepting a C source file.


## Compiler source layout

The compiler is organized by subsystem, following the broad source-tree organization used by production compilers such as Swift:

```text
compiler/
  include/swan/       Stable compatibility include paths
  lib/
    AST/              Abstract syntax tree and declarations
    Basic/            Core utilities, target model, linker support
    CodeGen/          Shared code generation and target backends
      Targets/
    Driver/            Compilation pipeline and orchestration
    IR/                Typed SVM IR, IR generation, optimization
    Parse/             UTF-8 lexer and parser
    SIL/               Swan Intermediate Language bridge and runtime passes
      Runtime/
    Sema/              Name/type checking and C header import
    Serialization/     Assembly output
  tools/
    swanc/             Production compiler command-line driver
```

Implementation headers live under `compiler/lib`. The small headers in `compiler/include/swan` forward to those implementations, preserving stable include paths for embedders and tests. The root `main.c` remains as a compatibility entry point.
