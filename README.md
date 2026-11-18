<p align="center"><img src="assets/swan-logo.png" alt="Swan" width="180"></p>

# Swan

Swan is a compiled programming language. Its production compiler, `swanc`, is implemented in C11 and owns the lexer, parser, semantic analysis, typed IR, optimizer, SVM pipeline, and native/cross-target backends. Swan source is compiled ahead of execution; it is not interpreted.

Licensed under Apache-2.0.

## Compiler system contract

The production architecture is specified in [docs/compiler_system.md](docs/compiler_system.md). The current implementation boundary and unfinished work are tracked in [the compiler completion matrix](docs/compiler_completion_matrix.md). The C11 implementation follows the code ownership, subsystem, diagnostics, and review standards in [docs/engineering_style.md](docs/engineering_style.md). The canonical portable path is source -> SVM typed IR -> WASM64 AOT -> SIC environment detection -> browser WASM execution or native Wasmtime JIT on x86-64/ARM64. The document distinguishes this path from the retained explicit native backends and records the remaining runtime and feature-parity limits.

## Language guide

The programmer-facing syntax, coding conventions, complete reserved-keyword list, operator precedence, standard library, and current feature boundaries are documented in [docs/language_guide.md](docs/language_guide.md).

## Current compiler

The compiler has two cooperating source-lowering paths:

- **Typed IR path:** preferred for the supported scalar/control-flow subset. Swan source is lowered directly from the AST through the general typed-IR generator. The resulting typed SSA IR is verified, optimized, and sent to a selected backend.
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
       general AST -> typed SSA IR (fallback)
       AST -> x86-64 compatibility backend (where required)
  -> SVM verification
  -> target-independent optimization
  -> WASM64 AOT emission (canonical portable path)
  -> SIC host detection:
       browser / WebAssembly -> keep and execute WASM64
       native x86-64 / ARM64 -> Wasmtime runtime JIT -> host machine code
  -> explicit legacy native backend (x86-64 / ARM64) only when selected
```

### Implemented typed-IR optimization

The optimizer runs a bounded, change-driven fixed-point pipeline (up to 350 individual pass executions) and performs width-aware constant folding, constant-select folding, CFG and constant-branch simplification, unreachable-block removal, algebraic simplification, trivial-PHI copy propagation, dominance-aware common-subexpression elimination, conservative loop-invariant code motion, post-hoisting CSE, and dead-code elimination. DCE propagates liveness through a value worklist. IR verification runs before optimization and after final cleanup; debug builds additionally verify every pass boundary. These passes apply to the typed-IR path; the AST compatibility backend has separate optimizations.

### SVM compiler core

SVM is the target-independent compilation engine, not a CPU target, runtime, or interpreter. It owns typed source lowering, IR verification, and optimization. The canonical `swanc compile` command emits WASM64 (memory64) ahead of execution. SIC then detects the host: browsers execute the WASM module directly, while native x86-64 and ARM64 hosts use Wasmtime's runtime JIT to generate host machine code. The explicit x86-64 and ARM64 AOT backends remain available as legacy/low-level targets, but they are not required for the canonical portable workflow.

The legacy `--target svm`, `swan vm`, `.swvm` serializer, and IR interpreter have been removed from the production CLI and target registry. SVM is the compiler engine shared by all compiled targets. See [SVM Compiler Architecture](docs/svm_architecture.md).

### Fail-fast behavior

The semantic checker, IR verifier, and code generators validate the constructs they receive. Unknown or unsupported nodes are diagnosed instead of silently becoming empty statements or zero-valued code. This does not mean every parser keyword is implemented: the editor vocabulary and token registry are broader than the currently supported semantic and backend subset.

Compilation is phase-gated: a failed read, lex, parse, import, semantic, IR verification, optimization, code-generation, assembly, or link step returns a non-zero status and prevents later phases from running. The compiler also emits non-fatal semantic warnings for suspicious constructs such as self-assignment, literal division by zero, constant conditions, potentially non-terminating constant-true loops, and source functions unreachable from `main`. Use `--warnings-as-errors` to stop before code generation whenever any warning is reported. Warnings are conservative bug indicators, not proof that the program is incorrect.

## High-level language direction

Swan is a high-level, general-purpose language. Ordinary programs use inferred types, managed values, strings, collections, functions, modules, and structured control flow. Developers do not manage object memory or hardware details manually.

Swan's intended default memory model is compiler/runtime-managed automatic reference counting (ARC). There is no raw import mode and no ordinary source-level ownership/borrowing or storage-placement syntax. The compiler may use stack, heap, registers, pooling, or other implementation strategies internally without exposing them as application-level requirements.

The public language direction and migration requirements are defined in [the High-Level Language Contract](docs/high_level_language.md). Fixed-width integer types remain available for explicit binary, FFI, and platform boundaries; they are not the default style for everyday code.

**Implementation status:** the compiler has not yet completed ARC, general collections, full type inference, or feature parity across targets. The [compiler completion matrix](docs/compiler_completion_matrix.md) is the source of truth for implemented behavior. A design requirement is not an implemented feature until its end-to-end tests pass.

## Install prebuilt compiler

Most developers should install a release binary. You do not need to clone this repository or build Swan from source.

**Linux and macOS**

```sh
curl -fsSL https://raw.githubusercontent.com/lstudiolab/Swan/main/scripts/install.sh | sh
swan --version
swanc compile examples/hello.swan -o hello.wasm
swan run hello.wasm
```

The installer detects Linux x86-64/ARM64 and macOS Intel/Apple Silicon, downloads the matching release archive, and installs `swan` and `swanc` into `~/.local/bin`. Ensure that directory is on PATH. Release binaries are published automatically for version tags.

**Windows 10/11 (x86-64)**

Run this in PowerShell:

```powershell
irm https://raw.githubusercontent.com/lstudiolab/Swan/main/scripts/install.ps1 | iex
```

Restart PowerShell, then use `swanc --version`. The Windows package includes the MSYS2 runtime DLL. Native executable linking currently requires the MSYS2 GCC toolchain (`cc`) on PATH; the compiler itself does not require the Swan source repository. Windows ARM64 is not packaged yet.

**Build from source (contributors)**

Requirements: a C11 compiler, POSIX shell, and a host assembler/linker available through `cc`.

```sh
sh build.sh
./build/swanc compile examples/hello.swan -o hello.wasm
./build/swanc run hello.wasm
```

Build an executable:

```sh
./build/swanc build examples/hello.swan -o hello
./hello
```

Emit x86-64 assembly for one source:

```sh
./build/swanc examples/hello.swan --emit-asm -o hello.s
```

Build one Swan source project for every supported target in one compiler invocation:

```sh
./build/swanc build app.swan --target all -o build/all
```

This parses and checks the Swan project once, then emits:

- `build/all/app-x86_64` — native x86-64 Linux executable
- `build/all/app-arm64.o` — ARM64 Linux ELF object; link it with an ARM64 toolchain before running it
- `build/all/app-wasm64.wasm` — WebAssembly memory64 module; run it with a memory64-capable runtime

The all-target build currently assumes an x86-64 host for its native x86-64 executable; producing the complete matrix from an ARM64 host still needs a configured x86-64 cross-linker and sysroot. It accepts Swan-only source projects. C ABI inputs, custom linker arguments, runtime-lazy imports, and `--no-heap` are not supported in this mode. The same source must use features supported by all three backends; target feature parity is not complete.

Emit one target artifact per Swan source in a multi-file project. The extension follows the selected backend (`.s` or `.wasm`):

```sh
./build/swanc build Sources/ --emit-asm -o build/assembly
./build/swanc build Sources/ --target aarch64 --emit-asm -o build/aarch64
./build/swanc build Sources/ --target wasm64 -o build/wasm
```

Native x86-64 `build` also assembles one object per Swan source and links them into one executable. Per-source cross-target emission creates independent artifacts with external references; combine them with a compatible target linker where that target supports linking. A multi-file cross-target `build` still emits the backend's project-level artifact.

Each source file is parsed independently, and native x86-64 builds compile separate objects before linking the project. Cross-target backends still use a project-wide IR artifact.

Inspect supported target names:

```sh
./build/swanc --list-targets
```

`build.sh` compiles the C11 production compiler directly into `build/swanc`. No bootstrap compiler or secondary compiler is built. `build/swan` remains a compatibility alias to the same executable.

## VS Code

The repository includes a local editor extension for .swan syntax highlighting, indentation, and snippets. It is not yet a language server.

Build and install the compiler on Linux or macOS:

~~~sh
sh build.sh
mkdir -p ~/.local/bin
cp build/swanc ~/.local/bin/swan
export PATH="$HOME/.local/bin:$PATH"
swan --version
~~~

Add the PATH export to your shell startup file so VS Code can find swan after restarting.

Build and install the VS Code extension:

~~~sh
cd vscode
npm install
npm run package
code --install-extension swan-language-0.1.0.vsix
~~~

Open a .swan file after reloading VS Code. The workspace build task uses the swan executable from PATH. On Windows, use WSL for the current POSIX compiler build.

## Swan syntax

Swan source uses indentation-based blocks introduced by a colon. Braces are not block delimiters. `func` is the canonical function keyword; `fn` remains a contextual compatibility spelling, not a reserved word. Semicolons are optional statement separators.

The exact reserved-keyword set is:

```text
func struct let var null if else const type match guard block while loop
true false break continue return as from self pass upto init deinit enum
throw throws rethrow catch
```

This is a 33-word, case-sensitive set. `import` and `export` are reserved words; `lazy` remains contextual. Other removed spellings (including `free`, `manual`, `defer`, `bool`, and `int`) are identifiers.

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
| AArch64 / ARM64 Linux | Native executable on an ARM64 host; Swan-owned ELF object/assembly when cross-compiling; signed i32/i64 operations, calls, and scalar C ABI linking |
| WebAssembly 64 | Swan-owned memory64 module emission for its implemented subset |
| SVM | Shared target-independent compilation engine; not a runtime target |

Object emission is not proof of execution or complete ABI integration on the destination platform. Cross-target runtime linking, full standard-library support, aggregate ABI lowering, and complete feature parity are not implemented.

### Cross-target examples

```sh
# On an x86-64 host: Swan-owned AArch64 / ARM64 ELF object
./build/swanc build app.swan --target arm64 -o app-aarch64.o

# On an ARM64 host: link and run natively
./build/swanc build app.swan --target arm64 -o app-arm64
./app-arm64

# Swan-owned WebAssembly memory64 module
./build/swanc build app.swan --target wasm64 -o app.wasm
```

Compile Swan source once to the portable WASM64 artifact:

```sh
swanc compile app.swan -o app.wasm
swan run app.wasm

# Or compile and run in one command
swan run app.swan --target wasm64
```

SIC selects Wasmtime's native JIT on supported x86-64/ARM64 hosts. Browser
deployments keep the WASM module and use [runtime/wasm64/browser.js](runtime/wasm64/browser.js)
to execute it through the browser's WebAssembly engine. Install Wasmtime
separately for native execution; see the [WASM64 runtime guide](runtime/wasm64/README.md).

Swan exposes exactly three code-generation targets: x86-64, AArch64/ARM64, and WASM64. ARMv7, RISC-V, WASM32, and arbitrary LLVM target triples are not supported. The compiler rejects other target names rather than silently routing them through another backend.

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
    print("Hello from Swan")
    print(abs_int(-42))
    return 0
```

`print(expression)` is the single standard-library output intrinsic. Foundation provides integer helpers `abs_int`, `min_int`, `max_int`, `clamp_int`, `sign_int`, `square_int`, `cube_int`, `power_int`, `factorial_int`, `sum_to_int`, `average_int`, `is_even`, `is_odd`, and `in_range_int`.

Set `SWAN_STDLIB` or pass `--stdlib <directory>` to select a standard-library directory.

## Standard library

Import the standard library explicitly and call public functions by their ordinary names:

```swan
import std

func main() -> int:
    print("Hello from Swan")
    print(abs(-42))
    print(max(10, 20))
    return 0
```

`print(expression)` is Swan's standard output call. Other `name(...)` calls resolve to ordinary functions in explicitly imported Swan modules. The current source modules are `std` (core integer helpers), `std_math` (integer number theory and arithmetic), and `std_collections` (algorithms over the currently supported `u32_array` type). Their public functions use ordinary function-call syntax. See [the standard-library reference](stdlib/readme.md) and [the language guide](docs/language_guide.md#121-standard-library-function-calls) for APIs and current limitations. The Swift and Goose standard-library surface is not yet complete: generic collections, strings/Unicode, floating-point math, OS and file APIs, serialization, concurrency, FFI-safe memory, graphics, audio, physics, and UI need language/runtime capabilities that are not currently implemented.

## Low-resource compilation

The compiler driver currently runs its compilation phases sequentially and does not create a compiler thread pool. Use `--low-resource` or `--jobs 1` to make the single-worker policy explicit:

```sh
./build/swanc build src/ --low-resource -o app
./build/swanc build src/ --jobs 1 -o app
```

Values above one are rejected until bounded parallel compilation is implemented. External toolchains and linkers are separate processes and may have their own resource behavior. See [Low-Resource Compilation](docs/low_resource_compilation.md).

## Data-center and backend runtime roadmap

Swan's planned backend runtime, JSON engine, SQL interface, concurrency model, networking stack, memory model, and performance acceptance gates are specified in [Data-Center Runtime and Backend Contract](docs/data_center_runtime.md). This is an implementation roadmap, not a claim that those APIs already exist. The current standard library does not yet provide production JSON, SQL, networking, or concurrency support. Features will be marked implemented only after end-to-end compiler, runtime, backend, and integration tests pass.

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

## Profiling and benchmarks

Compiler-stage profiling:

```sh
./build/swanc --profile examples/hello.swan
# or
./build/swanc run examples/hello.swan --profile
```

The report measures source reading, lexing, parsing, imports, semantic analysis, reachability, typed-IR generation, IR verification, total optimization, individual optimization passes, target code generation, assembly output, native linking, and execution where applicable. GitHub Actions also records peak compiler RSS for each workload.

Repeatable compiler pipeline benchmark:

```sh
python3 benchmarks/compiler_pipeline.py
```

The benchmark warms up and reports median timings across compiler stages for its included workloads. It is a measurement tool, not evidence of a particular performance ranking.

The repository also includes runtime comparison workloads, including `benchmarks/binary_search.py`. Run benchmarks on controlled hardware and compare checksums before comparing speed. Do not infer that Swan is faster than another compiler from compilation time alone.

For a heavy compile-time comparison across Goose, Swan, Rust, C++, C, and x86-64 assembly, see [the six-toolchain compilation benchmark](benchmarks/compilation.md). It generates large workloads and reports median compile-plus-link time and peak compiler memory.

## Tests

Run the complete repository test script:

```sh
sh test.sh
```

The script builds the single production compiler directly, verifies that no bootstrap/secondary compiler artifacts are produced, checks header isolation and C11 compilation, runs C ABI and UTF-8 tests, profiling tests, smoke/regression tests, Foundation and lazy-import tests, architecture/backend tests, and the keyword registry test. Cross-target object validation does not establish native execution or full ABI correctness on those targets.

## Repository map

- `main.c` — CLI orchestration and compiler entry point.
- `compiler/include/swan/` — stable compatibility/public headers.
- `compiler/lib/` — compiler implementation, organized by subsystem (AST, Basic, CodeGen, Driver, IR, Parse, Sema, Serialization).
- `stdlib/` — source-level standard library.
- `tests/` — compiler, runtime, ABI, regression, and target tests.
- `benchmarks/` — compiler and runtime measurement programs.
- `docs/` — architecture, pipeline, performance, audit, and testing notes.

## Documentation policy

The parser keyword registry is not a feature checklist. A feature is described as implemented only when its parsing, semantic rules, lowering, backend support, and tests establish that status. Target claims always state their supported subset. Performance claims require reproducible benchmark data.

## Whole-project compilation

A project directory is one program and one link graph, but **each source file remains its own translation unit and produces its own assembly artifact**. Ten `.swan` files produce ten Swan assembly files; there is no ten-files-to-one-assembly batching rule. Declarations are resolved across the project, while code generation retains each source file's identity. Native executable builds compile the per-source assembly into separate objects and link those objects into the final executable.

```text
project/
  main.swan
  math.swan
  physics.swan
  native.c
  native.h
```

```sh
# Emit one assembly file per Swan source into the output directory.
./build/swanc build project --emit-asm -o build/assembly
# Result: main.s, math.s, physics.s, and one native_N.s per C source.

# Or compile the translation units separately and link one executable.
./build/swanc build project -o build/project
```

The same works with explicit source lists:

```sh
./build/swanc build src/main.swan src/math.swan src/physics.swan src/native.c -o app
```

Directory discovery is currently non-recursive: place project source files in the selected directory, or pass nested source files explicitly. The project shares declaration and semantic analysis, not a combined assembly output. Each Swan source is emitted independently; imported modules follow the selected module-linking mode. Prebuilt object files and external system libraries remain normal linker inputs. Cross-target executable linking, C source bundling, and complete cross-platform C ABI interoperability are still being developed.

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
    Sema/              Name/type checking and C header import
    Serialization/     Assembly output
  tools/
    swanc/             Production compiler command-line driver
```

Implementation headers live under `compiler/lib`. The small headers in `compiler/include/swan` forward to those implementations, preserving stable include paths for embedders and tests. The root `main.c` remains as a compatibility entry point.
