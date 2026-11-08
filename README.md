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

The compiler accepts `--target <target>` and `--list-targets`. Target names and triples are centralized in `compiler/include/swan/target.h`. Backend availability is reported by `--list-targets`. AArch64 and RISC-V64 currently emit cross-target assembly or ELF relocatable objects for the supported scalar IR subset; they do not execute on the host. WebAssembly emits a module for straight-line scalar functions and can be run by a WebAssembly runtime. The portable SVM executes verified IR on the host and supports the broader control-flow subset. Unsupported IR is rejected with a diagnostic rather than silently lowered.

## Language subset

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

CI runs the complete suite, including compiler construction, header isolation, parser and semantic checks, native execution, arrays, loop control, ABI alignment, Foundation imports, and typed SSA verification, IR lowering, SVM execution, WebAssembly module execution, AArch64 and RISC-V64 object emission.

## Backend status

| Target | Status | Current scope |
|---|---|---|
| x86-64 System V Linux | Implemented | Full existing native compiler path |
| AArch64 / ARM64 | Implemented, limited | Scalar typed IR; emits AArch64 assembly and ELF objects |
| RISC-V 64 | Implemented, limited | Scalar typed IR; emits RV64 assembly and ELF objects |
| WebAssembly 32-bit | Implemented, limited | Straight-line scalar functions; emits WASM 1.0 modules |
| Portable SVM | Implemented | Verified IR execution, calls, mutable locals, loops and branches |
| ARMv7 / Thumb | Not implemented | Registry only |
| WebAssembly 64-bit / memory64 | Not implemented | Registry only |
| Windows x64 / Apple Darwin | Not implemented | Registry only |

Cross-target backends currently reject unsupported IR constructs. Their supported subset is narrower than the x86-64 native backend; array lowering, general multi-block control flow, runtime-lazy imports, and complete platform ABI support are not yet available on all targets. A listed implemented backend does not imply every Swan program can target it.

Swan is open source under Apache-2.0. Contributions and reproducible benchmark reports are welcome.
