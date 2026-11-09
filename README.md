# Swan

Swan is an open-source compiled programming language. Its compiler is implemented in C11 and owns its frontend and code generation. It does not transpile Swan source to C and does not delegate code generation to LLVM or Clang.

The repository is licensed under Apache License 2.0.

## Compiler pipeline

```text
.swan source
  -> C11 lexer
  -> indentation-aware parser
  -> standard-library module loader
  -> semantic analysis
  -> Swan AST
  -> typed, verified SSA IR (irgen.h)
  -> target-independent IR optimization
  -> selected backend:
       x86-64 System V | AArch64 | RISC-V64 | WebAssembly 32 | portable SVM
  -> Swan assembly / WebAssembly / SVM module
  -> target assembler and linker (native ELF targets)
```

The typed-IR optimizer currently runs width-aware constant folding, CFG simplification, algebraic simplification, trivial-PHI copy propagation, dominance-aware common-subexpression elimination, conservative loop-invariant code motion, a post-hoisting CSE pass, and dead-code elimination. Every pass is followed by IR verification. These passes apply only to the typed-IR path; the established AST compatibility backend retains its own optimizations.

`main.c` is the only compiled compiler translation unit. Compiler subsystems are C headers under `compiler/include/swan/`. The system assembler and linker only consume assembly already emitted by Swan.

## Build and run

Requirements: a C11 compiler and a Linux x86-64 assembler/linker available through `cc`.

```sh
sh build.sh
./build/swan run examples/hello.swan
```

Build an executable without running it:

```sh
./build/swan build examples/hello.swan -o hello
./hello
```

Emit assembly:

```sh
./build/swan examples/hello.swan --emit-asm -o hello.s
```

The compiler accepts `--target <target>` and `--list-targets`. Target names and triples are centralized in `compiler/include/swan/target.h`. Backend availability is reported by `--list-targets`. AArch64 and RISC-V64 emit ELF relocatable objects; ARMv7 emits 32-bit ARM ELF objects for its supported `i32` subset. These cross-target objects must be linked for their destination system and cannot execute on the x86-64 host. WebAssembly 32 emits executable modules with control flow, calls, PHI nodes and linear-memory-backed mutable locals. WebAssembly 64 emits memory64 modules for the same supported scalar/control-flow subset. The x86-64 compiler now prefers verified SSA IR for the supported scalar subset and retains the established AST backend as a compatibility path for arrays, logging and lazy imports. The portable SVM executes verified IR on the host and can also serialize and load standalone `.swvm` modules. Unsupported IR is rejected with a diagnostic rather than silently lowered.\n\nCross-target example: `./build/swan build examples/hello.swan --target aarch64 -o hello-aarch64.o`. For WebAssembly, use `--target wasm32 -o hello.wasm`; for portable bytecode, use `--target svm -o hello.swvm`.

## Compiler pipeline profiling

Run `build/swan --profile path/to/program.swan` (or place `--profile` after the source path) to print measured compiler-stage durations in seconds to stderr. The report separates source reading, lexing, parsing, imports, semantic analysis, reachability, IR generation, IR optimization, target code generation (or AST fallback), assembly output, native linking, and program execution. Stages not used by the selected target are omitted. `Total wall time` includes CLI setup and teardown; per-stage timings are measured around the actual stage calls. Use the same target, source, compiler build, and machine when comparing runs. For steadier measurements, run several times and compare the reported values; the first run may include filesystem and linker cold-start effects.

## Source encoding\n\nSwan source files are UTF-8 by default and are interpreted independently of the host locale. The lexer validates UTF-8 before tokenization, accepts an optional UTF-8 BOM, preserves UTF-8 bytes in string literals, and accepts non-ASCII identifiers. Malformed UTF-8 and embedded NUL bytes in source files are rejected. Identifier classification currently treats non-ASCII Unicode code points as identifier characters; full Unicode XID category tables and Unicode normalization are not implemented.\n\n## Language subset

The current x86-64 backend supports:

- Signed 64-bit integer and boolean expressions.
- Mutable and immutable local bindings.
- Functions and up to six call arguments.
- `if`, `else`, `while`, `break`, and `continue`.
- Arithmetic, comparisons, short-circuit `and` / `or`, and `not`.
- Constant folding, small single-return function inlining, register locals, direct local arithmetic, and loop strength reduction.
- Contiguous zero-initialized `u32` arrays.
- String literals in `log` and `echo!(...)` output statements.

Complete static type checking, floating-point execution, structs, general string values, closures, recursion-specific optimization, and more than six call arguments remain outside the implemented subset.

## Arrays and memory

`array(length)` and `array_u32(length)` allocate zero-initialized, bounds-checked `u32` arrays. The array length is stored in an eight-byte header immediately before the data.

`array_unchecked(length)` and `array_u32_unchecked(length)` allocate contiguous `u32` storage without per-access bounds checks. This is intended for code where the caller can prove every index is valid.

Both modes use the system allocator. The unchecked form avoids the eight-byte length header, keeping the data allocation compact.

## SAMC memory model

Swan 0.7.0 introduces **SAMC (Swan Automatic Memory Cleanup)**. SAMC uses compiler-tracked ownership and does not use runtime reference counters.

An array declared normally is automatically released when its owning function exits, including early returns:

```swan
fn main():
    let values = array(1024)
    values[0] = 42
    return 0
```

Use the `manual` declaration modifier to opt an allocation out of automatic cleanup. Release it explicitly with `free`:

```swan
fn main():
    manual let values = array(1024)
    values[0] = 42
    free(values)
    return 0
```

SAMC conservatively leaves arrays allocated when their ownership escapes through a return, alias assignment, or function argument. This prevents premature frees in code where the compiler cannot prove that the current function is the final owner. Those escaped allocations require an explicit ownership strategy; SAMC does not yet perform interprocedural lifetime analysis or per-block last-use cleanup. Manual allocations are never implicitly freed.

## Lazy imports and library tree shaking

Standard-library imports are resolved and type-checked during compilation, but imported code is no longer linked into the executable eagerly. Swan follows the call graph from `main`, builds only the imported functions reachable from that entry point into per-module shared libraries, and omits unused library functions.

Imported functions are called through small cached dispatch stubs. The first call resolves the function with `dlopen` / `dlsym`; subsequent calls jump directly to the cached function pointer. A module that is never called is never loaded, even if a call exists on a runtime branch that is not taken. The shared libraries are stored beside the executable in a `.modules/` directory, so keep that directory with the executable.

This applies to `swan build` and `swan run`. `--emit-asm` currently rejects programs that need runtime-lazy imported functions because a standalone assembly file cannot carry the generated module bundle and resolver runtime.

## Foundation standard library

Foundation is a source-level standard-library module under `stdlib/foundation.swan`.

```swan
import foundation

fn main():
    echo!("Hello from Swan")
    echo_int(abs_int(-42))
    return 0
```

`echo!("text")` is a compiler-recognized output intrinsic that emits a direct `puts` call for a string literal. Foundation also provides integer helpers such as `abs_int`, `min_int`, `max_int`, `clamp_int`, `factorial_int`, `sum_to_int`, `is_even`, and `is_odd`.

Set `SWAN_STDLIB` or pass `--stdlib <directory>` to use a different standard-library directory. The installer places Foundation beside the installed compiler and configures the `swan` launcher to find it.

## Benchmarks

Run the 20-million-element / 20-million-query comparison through GitHub Actions or locally with:

```sh
sh build.sh
python3 benchmarks/binary_search.py
```

The comparison checks every result against the same checksum and records compile time, runtime samples, peak RSS, and executable size. It includes checked and unchecked Swan, Goose, Rust, C, and C++.

The repository also contains larger sorting and multi-language workloads. The one-billion-element sort is intentionally excluded from ordinary CI because of its memory and runtime requirements.

## Tests

```sh
sh test.sh
```

CI runs the complete suite, including compiler construction, header isolation, parser and semantic checks, native execution, arrays, loop control, ABI alignment, Foundation imports, and typed SSA verification, optimization pass tests, IR lowering, SVM execution and module round-trips, WebAssembly 32/64 module validation, and AArch64, ARMv7 and RISC-V64 object emission.

## Backend status

| Target | Status | Current scope |
|---|---|---|
| x86-64 System V Linux | Implemented | Swan SIL -> typed SSA IR for supported scalar functions; typed-IR and AST compatibility fallbacks for broader constructs |
| AArch64 / ARM64 | Implemented, limited | Integer scalar IR, calls, CFG, PHI edges and mutable stack locals; emits AArch64 ELF objects |
| RISC-V 64 | Implemented, limited | Integer scalar IR, calls, CFG, PHI edges and mutable stack locals; emits RV64 ELF objects |
| WebAssembly 32-bit | Implemented, limited | Integer scalar IR, calls, CFG, PHI edges and linear-memory-backed mutable locals |
| Portable SVM | Implemented | Verified IR execution, calls, mutable locals, loops and branches |
| ARMv7 / ARM mode | Implemented, limited | `i32` scalar IR, calls, CFG, PHI edges and mutable stack locals; emits ARMv7 ELF objects |
| WebAssembly 64-bit / memory64 | Implemented, limited | Integer scalar IR, calls, CFG, PHI edges, memory64 stack frames and mutable locals |
| Windows x64 / Apple Darwin | Not implemented | Registry only |

Cross-target backends currently reject unsupported IR constructs. Their supported subset is narrower than the x86-64 frontend; array/slice lowering, floating-point arithmetic, runtime-lazy imports, and complete platform ABI support are not yet available on all targets. ARMv7 lowers integer division and remainder through the AAPCS EABI helpers with explicit zero and signed-overflow traps. Cross-target backends are intentionally a supported subset rather than claiming full frontend parity. A listed implemented backend does not imply every Swan program can target it.

Swan is open source under Apache-2.0. Contributions and reproducible benchmark reports are welcome.

## Swan-authored SIL pipeline

The experimental scalar SIL implementation lives in `compiler/sil/` and is written in Swan itself. Its current linear SSA record format uses five u32 cells per instruction: encoded opcode/type, destination, left operand, right operand, and immediate. The pipeline is:

```text
Swan SIL builder
  -> structural and typed SSA verification
  -> scalar constant folding
  -> backward value-demand / liveness analysis
  -> dead-instruction elimination and stream compaction
  -> final SIL verification
```

The passes share a caller-owned workspace and are compiled to native code by Swan in `tests/sil.sh`. This SIL implementation is currently a standalone experimental scalar IR pipeline. The production compiler's general typed SSA IR optimizer in `compiler/include/swan/optimization.h` remains the optimizer used by normal `.swan` compilation; the SIL passes have not yet replaced or been wired into that production IR path. The present SIL format is limited to 256 value IDs and a 256-instruction stream, and its packed constant builder currently accepts 16-bit immediate values.


## Native C ABI interoperability

Swan can declare external C ABI functions directly in source. External declarations
are represented as declaration-only functions in typed IR, so normal Swan calls
remain visible to SIL/IR verification, optimization, and native code generation.

```swan
extern fn c_add(left: i64, right: i64) -> i64

fn main() -> int:
    return c_add(20, 22)
```

Compile C code to an object file and pass it to the native linker:

```sh
cc -std=c11 -O2 -c library.c -o library.o
swan build app.swan --link library.o -o app
```

Additional object files and libraries can be supplied with repeated `--link`
options or standard `-L` and `-l` options. The current direct-call ABI subset
is scalar integer and boolean arguments/returns on the x86-64 System V ABI.
C-header parsing, aggregate layouts, floating-point ABI classification, and
additional platform calling conventions are not yet implemented.
